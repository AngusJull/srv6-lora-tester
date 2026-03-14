import serial
import argparse
import time
import datetime
import json

DATA_END_SEQUENCE = "}\n".encode()
RETREIVAL_COMMAND = "print_records"


def read_until_no_data(conn, timeout):
    end_time = time.time() + timeout
    received_data = bytearray()

    while True:
        if conn.in_waiting > 0:
            # Don't get the prompt when the cmd completes
            data: bytes = conn.read_until(DATA_END_SEQUENCE)
            received_data.extend(data)

            if data.endswith(DATA_END_SEQUENCE):
                print("Got end statement")
                break

            end_time = time.time() + timeout
            print(f"Got {len(data)} new bytes, now have {len(received_data)} bytes")
        else:
            if time.time() > end_time:
                break
            time.sleep(1)

    return bytes(received_data)


def collect(port):
    conn = serial.Serial(port, 115200, timeout=None)
    print("Serial connection opened")

    # Give the conn some time to open
    time.sleep(0.1)

    conn.reset_output_buffer()

    data = None
    while not data:
        print("Getting data from board")
        conn.write(RETREIVAL_COMMAND.encode())
        # Make sure we don't pick up echoed output
        conn.reset_output_buffer()
        conn.write("\n".encode())
        # Read until a certain sequence (pretty much hardcoded to our current format), or until timeout (more general)
        data = read_until_no_data(conn, 0.5).decode()

        if not data:
            print("Didn't get any data, sending command again")

    print(f"Got data {data}")
    try:
        json_data = json.loads(data)
    except Exception as e:
        print(f"Got an exception [{e}] decoding data, not storing any results")
        return

    return json_data


def config_changes(port):
    print("Change board configuration:")
    print("Change id: id <id>")
    print("Change topology: topo <id>")
    print("Enable/disable srv6: sr <id>")
    print("Enable/disable throughput test: tp <id>")
    print("Enter these on one line seperated by spaces, or nothing for no changes")
    changes = input("> ")

    if len(changes) != 0:
        conn = serial.Serial(port, 115200, timeout=0.1)
        print("Serial connection opened")
        time.sleep(0.1)

        conn.reset_output_buffer()
        conn.write(f"set_config {changes}\n".encode())
        print(f"Response: {conn.read_until('>\n'.encode())}")
        return True

    return False


def persist_data(data, filename):
    with open(filename, "w+") as file:
        json.dump(data, file)


def run_collection():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--file", required=False)
    args = parser.parse_args()

    if not args.file:
        time_str = datetime.datetime.now().strftime("%Y_%m_%d_%H_%M")
        args.file = f"recorder_results_{time_str}.json"

    collected_data = []
    while True:
        collected_data.append(collect(args.port))
        persist_data(collected_data, args.file)
        print("Saved data to file")

        while config_changes(args.port):
            pass

        _ = input("Press enter when ready for next board")


if __name__ == "__main__":
    run_collection()
