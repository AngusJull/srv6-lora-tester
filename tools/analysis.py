import json
import argparse
import polars


def run_analysis():
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", required=True)

    args = parser.parse_args()

    with open(args.file, "r") as file:
        data = json.load(file)

    dataframes = transform_to_dataframe(data)
    return dataframes


def transform_to_dataframe(collected_data: list):
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

            try:
                dataframes[record_type].vstack(df, in_place=True)
            except:
                breakpoint()

    return dataframes


if __name__ == "__main__":
    dataframes = run_analysis()
