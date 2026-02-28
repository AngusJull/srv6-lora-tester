import serial
import argparse
import time
import datetime
import json

DATA_END_SEQUENCE = "}\n"
RETREIVAL_COMMAND = "dump_buffer"


def collect(port, into: dict):
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

    current_node_data = into.get(json_data["node_id"], dict())
    # If we just made a new dict, need to assign to the key
    into[json_data["node_id"]] = current_node_data

    for key in json_data:
        # Keep everything but node id under the node id as a key
        if key == "node_id":
            continue
        # Assume that beneath the first level of keys, the data is a bunch of lists,
        # append in case we just read more data form the same node
        current_node_data[key] = current_node_data.get(key, []) + json_data[key]

    print(f"Got data {json_data}")


def persist_data(data, filename):
    with open(filename, "w+") as file:
        json.dump(data, file)


def run_processing(args):
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", required=True)
    args = parser.parse_args(args)


def run_collection(args):
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--file", required=False)
    args = parser.parse_args(args)

    if not args.file:
        time_str = datetime.datetime.now().strftime("%Y_%m_%d_%H_%M")
        args.file = f"recorder_results_{time_str}"

    collected_data = dict()
    while True:
        collect(args.port, collected_data)
        persist_data(collected_data, args.file)
        print("Saved data to file")
        _ = input("Press enter when ready for next board")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["collect", "process"])
    args, remaining_args = parser.parse_known_args()

    if args.command == "collect":
        run_collection(remaining_args)
    elif args.command == "process":
        run_processing(remaining_args)
