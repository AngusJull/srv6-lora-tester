import json
import argparse
import polars
import numpy

LATENCY_RECORD_FAILED_TYPE = 0
LATENCY_RECORD_SUCCESS_TYPE = 1

CAPTURE_TYPE_SIXLOWPAN = 0
CAPTURE_TYPE_IPV6 = 1
CAPTURE_TYPE_SRV6 = 2

# Take off the earliest 5000ms of data from each node to avoid contamination from network setup
SKIM_EARLY_MS = 2000

# Take off the latest data from each node, to avoid contamination from the USB being plugged in
SKIM_LATE_MS = 15000


def run_analysis():
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", required=True)
    args = parser.parse_args()

    print(f"For file {args.file}")

    data = load_data(args.file)
    dataframes = transform_to_dataframe(data)

    skim_and_analyze(dataframes)

    print()

    return dataframes


def load_data(filename: str):
    with open(filename, "r") as file:
        data = json.load(file)

    return data


def transform_to_dataframe(collected_data: list) -> dict:
    """
    Aggregate the data into dataframes to make working with it easier
    """
    dataframes: dict = dict()

    # Data is stored in the same way it comes off the board, reorganize into dataframes
    for collection in collected_data:
        # Not sure how null gets into the json data
        if collection is None:
            continue
        for record_type in collection:
            if record_type in [
                "node_id",
                "topology_id",
                "use_srv6",
                "throughput_test",
            ]:
                continue

            if record_type not in dataframes:
                dataframes[record_type] = polars.DataFrame()

            # If missing this data, no need to add it
            if len(collection[record_type]) == 0:
                continue

            df = polars.DataFrame(collection[record_type])
            df = df.with_columns(node_id=collection["node_id"])
            dataframes[record_type].vstack(df, in_place=True)

    return dataframes


def avg_rtt_sender(sender_id: int, latency_records) -> tuple[float, float]:
    valid_records = latency_records.filter(
        polars.col("type") == LATENCY_RECORD_SUCCESS_TYPE
    )["rt_time"]
    mean = valid_records.mean()
    std = valid_records.std()
    return mean, std


def battery_change(stats_records) -> dict:
    out = dict()
    for node_id, group_df in stats_records.group_by("node_id"):
        x = group_df["time"].to_numpy().reshape(-1)
        y = group_df["mv"].to_numpy()

        # Fit linear regression to get rate of change and ignore noise
        coef = numpy.polyfit(x, y, 1)
        # We get result in units of mv/ms, so turn that into mv/s
        out[node_id[0]] = float(coef[0] * 1000)

    return out


def avg_battery_change(stats_records):
    changes = battery_change(stats_records)
    sum = 0
    for change in changes.values():
        sum += change

    return sum / len(changes)


# Get the size of packets at the L3 level
def avg_l3_size(capture_records) -> tuple[float, float]:
    avg = capture_records.filter(
        (polars.col("pkt_type") == CAPTURE_TYPE_IPV6).or_(
            polars.col("pkt_type") == CAPTURE_TYPE_SRV6
        )
    ).mean()

    # Calculate average L3 size so we know how much SRH has increased our packet size
    return avg["hdr_len"][0] + avg["pld_len"][0]


# Get the number of bytes L2 packets use to transmit one L3 packet, on average
def avg_l2_size(capture_records):
    sums = capture_records.filter(
        (polars.col("pkt_type") == CAPTURE_TYPE_SIXLOWPAN)
    ).sum()

    # Need number of packets from L3 level because fragmentation does not produce the exact same size of packet every time
    count = len(
        capture_records.filter(
            (polars.col("pkt_type") == CAPTURE_TYPE_IPV6).or_(
                polars.col("pkt_type") == CAPTURE_TYPE_SRV6
            )
        )
    )

    return (sums["hdr_len"][0] + sums["pld_len"][0]) / count


# Get the size of packets at the L3 level and L2 level and compare
def compression_ratio(capture_records):
    return avg_l3_size(capture_records) / avg_l2_size(capture_records)


def fragmentation_ratio(capture_records):
    # Get # of packets sent at L3 and L2 level, compare
    # Need to use capture instead of stats bc. other messages would mess with ratios
    counts = capture_records["pkt_type"].value_counts()
    l2_count = counts.filter(polars.col("pkt_type") == CAPTURE_TYPE_SIXLOWPAN)["count"][
        0
    ]
    l3_count = counts.filter(
        (polars.col("pkt_type") == CAPTURE_TYPE_IPV6).or_(
            polars.col("pkt_type") == CAPTURE_TYPE_SRV6
        )
    )["count"][0]
    return l2_count / l3_count


def l3_payload_size(sender_id, capture_records):
    return capture_records.filter(
        (polars.col("pkt_type") == CAPTURE_TYPE_IPV6)
        .or_(polars.col("pkt_type") == CAPTURE_TYPE_SRV6)
        .and_(polars.col("node_id") == sender_id)
    ).mean()["pld_len"][0]


def throughput(sender_id, num_hops, l3_payload_size, latency_records):
    # Just latency put differently and with payload taken into account

    # Round trip time is in milliseconds here
    rtt = avg_rtt_sender(sender_id, latency_records)[0] / 1000

    # Return value in (hops * bytes)/second, to get an idea of how fast the transmission was for each hop
    return (l3_payload_size * num_hops) / (rtt / 2)


def latency(sender_id, num_hops, latency_records):
    # Return value in (seconds) / hop

    # Round trip time is in milliseconds here
    rtt = avg_rtt_sender(sender_id, latency_records)[0] / 1000

    return (rtt / 2) / num_hops


def senders_list(latency_records) -> list:
    return list(latency_records["node_id"].unique())


def filter_early_and_late_samples(dataframe, skim_head_ms, skim_tail_ms):
    time_extrema_df = dataframe.group_by("node_id").agg(
        polars.col("time").min().alias("min_time"),
        polars.col("time").max().alias("max_time"),
    )

    df_with_extrema = dataframe.join(time_extrema_df, on="node_id")
    return df_with_extrema.filter(
        (polars.col("time") >= (polars.col("min_time") + skim_head_ms)).and_(
            polars.col("time") <= (polars.col("max_time") + skim_tail_ms)
        )
    )


def print_analysis(latency_records, capture_records, stats_records):
    senders = senders_list(latency_records)
    for sender in senders:
        print(f"For sender {sender}")
        hops = int(input("Enter number of hops: "))
        latency_val = latency(sender, hops, latency_records)
        print(f"{'  latency (seconds / hop)':<50} {latency_val}")
        throughput_val = throughput(
            sender, hops, l3_payload_size(sender, capture_records), latency_records
        )
        print(f"{'  throughput (hop * payload bytes / second)':<50} {throughput_val}")

    print(
        f"{'Fragmentation ratio (l2 count / l3 count)':<50} {fragmentation_ratio(capture_records)}"
    )
    print(
        f"{'Compression ratio (l3 size / l2 size)':<50} {compression_ratio(capture_records)}"
    )
    # We get the payload size from one sender, assuming all nodes use the same packet size for now
    print(
        f"{'Overhead ((l2 bytes per l3 packet) / payload bytes':<50} {avg_l2_size(capture_records) / l3_payload_size(senders[0], capture_records)}"
    )
    print(f"{'Average L2 packet size (bytes)':<50} {avg_l2_size(capture_records)}")
    print(
        f"{'Average rate of change in battery (mv/s)':<50} {avg_battery_change(stats_records)}"
    )


def skim_and_analyze(dataframes):
    # We need to skim the stats records because battery is contaminated by the USB being plugged in to get data
    skimed_stats = filter_early_and_late_samples(
        dataframes["stats_records"], SKIM_EARLY_MS, SKIM_LATE_MS
    )
    print_analysis(
        dataframes["latency_records"], dataframes["capture_records"], skimed_stats
    )


if __name__ == "__main__":
    dataframes = run_analysis()
