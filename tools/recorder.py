import serial
import argparse
import time
import datetime
import json

DATA_END_SEQUENCE = "}\n"
RETREIVAL_COMMAND = "print_records"


def collect(port):
    conn = serial.Serial(port, 115200, timeout=1)
    print("Serial connection opened")

    # Give the conn some time to open
    time.sleep(0.1)

    conn.reset_output_buffer()

    print("Getting data from board")
    conn.write(RETREIVAL_COMMAND.encode())
    # Make sure we don't pick up echoed output
    conn.reset_output_buffer()
    conn.write("\n".encode())
    # Read until a certain sequence (pretty much hardcoded to our current format), or until timeout (more general)
    data = conn.read_until(DATA_END_SEQUENCE.encode()).decode()

    print(f"Got data {data}")
    try:
        json_data = json.loads(data)
    except Exception as e:
        print(f"Got an exception [{e}] decoding data, not storing any results")
        return

    return json_data


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
        _ = input("Press enter when ready for next board")


if __name__ == "__main__":
    run_collection()
