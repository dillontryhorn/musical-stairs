#!/usr/bin/env python3
"""Loop MIDI notes through a Casio CT-S190

Setup:
  1. Connect the keyboard to the computer with a micro-USB DATA cable.
  2. Power on the keyboard. Turn up the keyboard's speaker volume if necessary.
  3. Install dependencies: python -m pip install mido python-rtmidi
  4. List outputs: python casio_midi_loop.py --list
  5. Play: python casio_midi_loop.py --port "CASIO" --bpm 120

Press Ctrl+C to stop. MIDI commands produce sound on the keyboard.
Run with --help for note, instrument, channel, and repeat settings.

Examples:
  python casio_midi_loop.py
  python casio_midi_loop.py --port 0 --notes 60 64 67 72 --bpm 100
  python casio_midi_loop.py --bank 2 --program 0 --loops 4

The default bank 0 / program 0 is GM PIANO 1. Bank 2 / program 0 selects
the CT-S190's STEREO GRAND PIANO. Program numbers are MIDI values 0-127,
not the keyboard's displayed tone numbers. Channel arguments use 1-16.

References:
  https://support.casio.com/global/en/emi/manual/CT-S190/
  https://mido.readthedocs.io/en/stable/
"""

import argparse
import math
import sys
import time

DEFAULT_NOTES = [60, 62, 64, 65, 67, 69, 71, 72]  # C4 through C5


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Repeat MIDI notes on a Casio keyboard. Ctrl+C stops playback.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--list", action="store_true", help="list MIDI outputs and exit")
    parser.add_argument("--port", help="output index, exact name, or unique name fragment")
    parser.add_argument("--bpm", type=float, default=120, help="tempo, 1-600 beats/minute")
    parser.add_argument("--channel", type=int, default=1, help="MIDI channel, 1-16")
    parser.add_argument("--velocity", type=int, default=80, help="note strength, 1-127")
    parser.add_argument("--bank", type=int, default=0, help="bank select MSB, 0-127")
    parser.add_argument("--program", type=int, default=0, help="MIDI program number, 0-127")
    parser.add_argument("--loops", type=int, default=0, help="repeat count, 0 means forever")
    parser.add_argument(
        "--notes", type=int, nargs="+", default=DEFAULT_NOTES,
        help="MIDI note numbers, 0-127, each note occupies one beat",
    )
    args = parser.parse_args(argv)
    for name, minimum, maximum in (
        ("channel", 1, 16), ("velocity", 1, 127), ("bank", 0, 127), ("program", 0, 127)
    ):
        if not minimum <= getattr(args, name) <= maximum:
            parser.error(f"--{name} must be between {minimum} and {maximum}")
    if not math.isfinite(args.bpm) or not 1 <= args.bpm <= 600:
        parser.error("--bpm must be between 1 and 600")
    if args.loops < 0:
        parser.error("--loops must be 0 (forever) or a positive integer")
    if any(not 0 <= note <= 127 for note in args.notes):
        parser.error("--notes must contain MIDI numbers between 0 and 127")
    return args


def select_port(names, selector):
    if not names:
        raise ValueError("No MIDI outputs found. Check keyboard power and the USB data cable.")
    if selector in names:
        return selector
    if selector is not None and selector.isdecimal():
        index = int(selector)
        if index < len(names):
            return names[index]
        raise ValueError("Port index is out of range. Run with --list.")
    fragment = "casio" if selector is None else selector.casefold()
    matches = [name for name in names if fragment in name.casefold()]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise ValueError("No matching MIDI output. Run with --list, then supply --port.")
    raise ValueError("Multiple MIDI outputs match. Use an exact name or index from --list.")


def wait_until(deadline):
    """Use absolute deadlines so send/print overhead does not accumulate per note."""
    remaining = deadline - time.monotonic()
    if remaining > 0:
        time.sleep(remaining)


def play_loop(port, mido, args):
    channel = args.channel - 1  # Mido uses 0-15
    beat = 60.0 / args.bpm
    active_note = None
    try:
        # Set only the selected channel. GM bank 0 / program 0 is a piano.
        for controller, value in ((64, 0), (7, 100), (11, 127), (0, args.bank)):
            port.send(mido.Message("control_change", channel=channel,
                                   control=controller, value=value))
        port.send(mido.Message("program_change", channel=channel, program=args.program))

        completed = 0
        deadline = time.monotonic()
        while args.loops == 0 or completed < args.loops:
            print(f"Loop {completed + 1}", flush=True)
            for note in args.notes:
                # If the computer was suspended or delayed, resume without
                # trying to send a burst of all the missed notes.
                now = time.monotonic()
                if now >= deadline + beat:
                    deadline = now
                active_note = note
                port.send(mido.Message("note_on", channel=channel,
                                       note=note, velocity=args.velocity))
                wait_until(deadline + beat * 0.8)
                port.send(mido.Message("note_off", channel=channel, note=note, velocity=0))
                active_note = None
                deadline += beat
                wait_until(deadline)  # The last 20% of the beat is a short rest.
            completed += 1
    finally:
        # Release an interrupted note and disable sustain before closing the
        # output. Cleanup also runs on Ctrl+C or a playback error.
        cleanup = [mido.Message("control_change", channel=channel, control=64, value=0)]
        if active_note is not None:
            cleanup.append(mido.Message("note_off", channel=channel,
                                        note=active_note, velocity=0))
        cleanup.extend([
            mido.Message("control_change", channel=channel, control=123, value=0),
            mido.Message("control_change", channel=channel, control=120, value=0),
        ])
        failed = False
        for message in cleanup:
            try:
                port.send(message)
            except Exception:
                # A disconnected device may no longer accept cleanup messages.
                failed = True
        if failed:
            print("Could not send all stop messages. If a note is stuck, restart the keyboard.",
                  file=sys.stderr)


def main(argv=None):
    args = parse_args(argv)
    try:
        import mido
        import rtmidi
    except ImportError:
        print("Install dependencies: python -m pip install mido python-rtmidi", file=sys.stderr)
        return 1

    try:
        mido.set_backend("mido.backends.rtmidi")
        names = mido.get_output_names()
        if args.list:
            print("MIDI output ports:")
            for index, name in enumerate(names):
                print(f"  {index}: {name}")
            if not names:
                print("  (None found, check keyboard power and the USB data cable)")
            return 0

        name = select_port(names, args.port)
        with mido.open_output(name) as port:
            print(f"Sending to: {name}")
            print(f"Channel {args.channel}, {args.bpm:g} BPM, notes {args.notes}")
            print("Press Ctrl+C to stop.")
            play_loop(port, mido, args)
        print("Finished.")
        return 0
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except Exception as exc:
        print(f"MIDI error: {exc}", file=sys.stderr)
        print("Check the connection and close other programs using the MIDI port.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())



