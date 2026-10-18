// This C File's Header
#include "usb_midi.h"

// 3rd Party Headers
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"

// The bulk of the USB connection logic is done in this file
// Dillon Tryhorn, 2026

static const char *TAG = "usb_midi";
static usb_host_client_handle_t client;
static usb_device_handle_t device;
static usb_transfer_t *transfer;
static int interface_number = -1;
static uint8_t endpoint, new_address;
static bool busy, closing, cancel_requested;
static TickType_t sent_at;

// Log USB operation errors and return whether the operation succeeded
static bool check(esp_err_t err, const char *operation)
{
    // Log the operation name and error details if the USB operation failed
    if (err != ESP_OK) 
	{
        ESP_LOGE(TAG, "%s: %s", operation, esp_err_to_name(err));
    }
    return err == ESP_OK;
}

// Process USB host library events continuously in a background task
static void host_task(void *arg)
{
    // Keep processing USB host events for as long as the task is running
    while(1) 
	{
        uint32_t flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
    }
}

// Track new USB device addresses and detect keyboard disconnections
static void device_event(const usb_host_client_event_msg_t *event, void *arg)
{
    // Save a newly connected USB device address for the next poll
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV) 
	{
        new_address = event->new_dev.address;
    }
    // Clean up if the disconnected device is the current keyboard
    else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE && event->dev_gone.dev_hdl == device) 
	{
        closing = true;
        ESP_LOGI(TAG, "Keyboard disconnected");
    }
}

// Clear the busy flag after a MIDI transfer and flag the keyboard for cleanup on failure
static void transfer_done(usb_transfer_t *done)
{
    busy = false;
    // Clean up if a transfer fails or has an unexpected byte count while not already closing
    if (!closing && (done->status != USB_TRANSFER_STATUS_COMPLETED || done->actual_num_bytes != 4)) 
	{
        ESP_LOGE(TAG, "MIDI transfer failed (status %d); reconnect keyboard", done->status);
        closing = true;
    }
}

// Find a USB MIDI 1.0 streaming interface and its bulk OUT endpoint
static bool find_endpoint(const usb_config_desc_t *config)
{
    const uint8_t *bytes = (const uint8_t *)config;
    int candidate = -1;
    // Walk through the configuration descriptors while a descriptor header still fits
    for (int offset = 0; offset + 2 <= config->wTotalLength;) 
	{
        const uint8_t *desc = bytes + offset;
        // Stop scanning if a descriptor is too short or extends past the configuration
        if (desc[0] < 2 || offset + desc[0] > config->wTotalLength) 
		{
            break;
        }
        // Check each interface descriptor for a compatible USB MIDI streaming interface
        if (desc[1] == USB_B_DESCRIPTOR_TYPE_INTERFACE) 
		{
            candidate = -1;
            // Reject an interface descriptor that is too short to read safely
            if (desc[0] < USB_INTF_DESC_SIZE) 
			{
                return false;
            }
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)desc;
			
            // Keep the interface number only for MIDI 1.0 streaming when alternate setting is zero
            candidate = (intf->bInterfaceClass == 1 && intf->bInterfaceSubClass == 3 &&
                         intf->bInterfaceProtocol == 0 && intf->bAlternateSetting == 0)
                        ? intf->bInterfaceNumber : -1;
        }
        // Inspect valid endpoint descriptors belonging to the candidate MIDI interface
        else if (candidate >= 0 && desc[1] == USB_B_DESCRIPTOR_TYPE_ENDPOINT && desc[0] >= USB_EP_DESC_SIZE) 
		{
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)desc;
            // Select a nonzero bulk OUT endpoint that can carry a four-byte MIDI packet
            if ((ep->bmAttributes & 3) == USB_TRANSFER_TYPE_BULK &&
                !USB_EP_DESC_GET_EP_DIR(ep) && USB_EP_DESC_GET_EP_NUM(ep) != 0 && USB_EP_DESC_GET_MPS(ep) >= 4) 
			{
                interface_number = candidate;
                endpoint = ep->bEndpointAddress;
                return true;
            }
        }
        offset += desc[0];
    }
    return false;
}

// Open the keyboard, claim its MIDI interface, and prepare a USB transfer
static void open_keyboard(uint8_t address)
{
    // Stop setup if the USB device cannot be opened
    if (!check(usb_host_device_open(client, address, &device), "Open keyboard")) 
	{
        return;
    }
	
    const usb_config_desc_t *config;
    // Clean up if descriptors cannot be read or no compatible MIDI endpoint is found
    if (!check(usb_host_get_active_config_descriptor(device, &config), "Read descriptors") || !find_endpoint(config)) 
	{
        ESP_LOGW(TAG, "No USB MIDI 1.0 bulk OUT interface");
        closing = true;
        return;
    }
    int selected_interface = interface_number;
    interface_number = -1; // Only release the interface after a successful claim.
	
    // Clean up if the MIDI interface cannot be claimed
    if (!check(usb_host_interface_claim(client, device, selected_interface, 0), "Claim MIDI")) 
	{
        closing = true;
        return;
    }
    interface_number = selected_interface;
	
    // Clean up if memory cannot be allocated for the MIDI transfer
    if (!check(usb_host_transfer_alloc(4, 0, &transfer), "Allocate MIDI transfer")) 
	{
        closing = true;
        return;
    }
    transfer->device_handle = device;
    transfer->bEndpointAddress = endpoint;
    transfer->callback = transfer_done;
    transfer->num_bytes = 4;
    ESP_LOGI(TAG, "Keyboard ready: interface %d, OUT endpoint 0x%02x", interface_number, endpoint);
}

// Cancel pending MIDI transfers and release keyboard resources once transfers finish
static void close_keyboard(void)
{
    // Halt and flush an active transfer if cancellation has not already been requested
    if (busy && !cancel_requested) 
	{
        check(usb_host_endpoint_halt(device, endpoint), "Halt MIDI");
        cancel_requested = check(usb_host_endpoint_flush(device, endpoint), "Cancel MIDI");
    }
    // Wait for transfer_done to finish the asynchronous cancellation before freeing resources
    if (busy) 
	{
        return;
    }
	
    // Release the MIDI interface only if it was successfully claimed
    if (interface_number >= 0) 
	{
        // Leave cleanup pending so a failed interface release can be retried
        if (!check(usb_host_interface_release(client, device, interface_number), "Release MIDI")) 
		{
            return;
        }
        interface_number = -1;
    }
    usb_host_transfer_free(transfer);
    transfer = NULL;
	
    // Leave cleanup pending so a failed device close can be retried
    if (!check(usb_host_device_close(client, device), "Close keyboard")) 
	{
        return;
    }
    device = NULL;
    closing = cancel_requested = false;
}

// Initialize the USB host, register device callbacks, and start the host event task
void usb_midi_init(void)
{
    const usb_host_config_t host_config = {.intr_flags = ESP_INTR_FLAG_LEVEL1};
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    const usb_host_client_config_t client_config = {
        .max_num_event_msg = 5,
        .async.client_event_callback = device_event,
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_config, &client));
    // Report a memory error if the USB host event task cannot be created
    ESP_ERROR_CHECK(xTaskCreate(host_task, "usb_host", 4096, NULL, 2, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

// Process USB device events, handle MIDI timeouts, and open or close the keyboard as needed
void usb_midi_poll(void)
{
    esp_err_t err = usb_host_client_handle_events(client, 0);
	
    // Check event handling errors while ignoring the normal timeout when no events are waiting
    if (err != ESP_ERR_TIMEOUT) 
	{
        check(err, "USB events");
    }
	
    // Host transfers have no built-in timeout. Cancel a keyboard that stops responding
    if (busy && !closing && xTaskGetTickCount() - sent_at >= pdMS_TO_TICKS(1000)) 
	{
        ESP_LOGE(TAG, "MIDI transfer timed out; reconnect keyboard");
        closing = true;
    }
	
    // Continue keyboard cleanup before handling any new connection
    if (closing) 
	{
        close_keyboard();
        return;
    }
	
    // Open a newly detected device when no keyboard is currently connected
    if (!device && new_address) 
	{
        uint8_t address = new_address;
        new_address = 0;
        open_keyboard(address);
    }
}

// Check whether the keyboard is connected with a prepared transfer and is not closing
bool usb_midi_ready(void)
{
    return device && transfer && !closing;
}

// Validate and submit a three-byte MIDI message when the keyboard is ready and not busy
bool usb_midi_send(uint8_t status, uint8_t note, uint8_t velocity)
{
    uint8_t type = status >> 4;
	
    // Reject unavailable or busy keyboards, unsupported message types, and data bytes above 127
    if (!usb_midi_ready() || busy || type < 8 || type > 14 || type == 12 || type == 13 || note > 127 || velocity > 127) 
	{
        return false;
    }
	
    // USB MIDI event: cable 0 + Code Index Number, followed by three MIDI bytes
    transfer->data_buffer[0] = type;
    transfer->data_buffer[1] = status;
    transfer->data_buffer[2] = note;
    transfer->data_buffer[3] = velocity;
	
    // Clean up and report failure if the MIDI transfer cannot be submitted
    if (!check(usb_host_transfer_submit(transfer), "Send MIDI")) 
	{
        closing = true;
        return false;
    }
    busy = true;
    sent_at = xTaskGetTickCount();
    return true;
}
