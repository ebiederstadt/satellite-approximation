from sentinelhub import (
    SentinelHubCatalog,
    BBox,
    CRS,
    SHConfig,
    DataCollection,
    bbox_to_dimensions,
    MimeType,
    SentinelHubRequest,
)
from configparser import ConfigParser
from dataclasses import dataclass
from datetime import datetime
import logging
import os
import tarfile
from pathlib import Path
from enum import Enum
import json

logger = logging.getLogger(__name__)


def get_available_dates(bbox, start_date, end_date, collection, config):
    catalog = SentinelHubCatalog(config)

    time_interval = start_date, end_date
    if collection == Collection.Sentinel1:
        collection_str = "sentinel-1-grd"
    elif collection == Collection.S:
        collection_str = "sentinel-2-l2a"
    search_iterator = catalog.search(
        collection_str,
        bbox=bbox,
        time=time_interval,
        fields={"include": ["id", "properties.datetime"], "exclude": []},
    )
    results = list(
        map(lambda x: x["properties"]["datetime"].split("T")[0], search_iterator)
    )
    return list(set(results))


@dataclass
class SentinelInfo:
    config: SHConfig
    resolution: int


def setup_sentinel(app_config) -> SentinelInfo:
    CLIENT_ID = app_config.get("sentinel", "client_id", raw=True)
    CLIENT_SECRET = app_config.get("sentinel", "client_secret", raw=True)

    sentinel_info = SentinelInfo(SHConfig(), resolution=10)

    if CLIENT_ID and CLIENT_SECRET:
        sentinel_info.config.sh_client_id = CLIENT_ID
        sentinel_info.config.sh_client_secret = CLIENT_SECRET

    return sentinel_info


def get_folder_from_bbox(bbox, app_config, lat_long_precision=7):
    """Convert a bounding box into a folder on the disk"""

    return Path(app_config["data"]["cache_dir"]).joinpath(
        "bbox" + "_".join(str(x)[: 5 + lat_long_precision] for x in bbox)
    )


class Collection(Enum):
    Sentinel1 = 0
    Sentinel2 = 1
    DEM = 3


output_shapes = {
    "RGB": {"id": "RGB", "bands": 3, "sampleType": "UINT16"},
    "B02": {"id": "B02", "bands": 1, "sampleType": "UINT16"},
    "B03": {"id": "B03", "bands": 1, "sampleType": "UINT16"},
    "B04": {"id": "B04", "bands": 1, "sampleType": "UINT16"},
    "B08": {"id": "B08", "bands": 1, "sampleType": "UINT16"},
    "B11": {"id": "B11", "bands": 1, "sampleType": "UINT16"},
    "CLP": {"id": "CLP", "bands": 1, "sampleType": "UINT8"},
    "SCL": {"id": "SCL", "bands": 1, "sampleType": "UINT8"},
    "DEM": {"id": "DEM", "bands": 1, "sampleType": "FLOAT32"},
    "VV": {"id": "VV", "bands": 1},
    "VH": {"id": "VH", "bands": 1},
    "CLD": {"id": "CLD", "bands": 1, "sampleType": "UINT8"},
    "sunAzimuthAngles": {"id": "sunAzimuthAngles", "bands": 1, "sampleType": "FLOAT32"},
    "viewAzimuthMean": {"id": "viewAzimuthMean", "bands": 1, "sampleType": "FLOAT32"},
    "sunZenithAngles": {"id": "sunZenithAngles", "bands": 1, "sampleType": "FLOAT32"},
    "viewZenithMean": {"id": "viewZenithMean", "bands": 1, "sampleType": "FLOAT32"},
}

evaluate_pixels = {
    "RGB": "RGB: [16*sample.B04, 16*sample.B03, 16*sample.B02]",
    "B02": "B02: [sample.B02]",
    "B03": "B03: [sample.B03]",
    "B04": "B04: [sample.B04]",
    "B08": "B08: [sample.B08]",
    "B11": "B11: [sample.B11]",
    "CLP": "CLP: [sample.CLP]",
    "SCL": "SCL: [sample.SCL]",
    "VV": "VV: [sample.VV]",
    "VH": "VH: [sample.VH]",
    "DEM": "DEM: [sample.DEM]",
    "CLD": "CLD: [sample.CLD]",
    "sunAzimuthAngles": "sunAzimuthAngles: [sample.sunAzimuthAngles]",
    "viewAzimuthMean": "viewAzimuthMean: [sample.viewAzimuthMean]",
    "sunZenithAngles": "sunZenithAngles: [sample.sunZenithAngles]",
    "viewZenithMean": "viewZenithMean: [sample.viewZenithMean]",
}


def create_request(
    config, bounding_box, resolution, output_ids, catalog_date, collection: Collection
):
    bbox = bounding_box
    size = bbox_to_dimensions(bbox, resolution=resolution)
    if size[0] > 2500 or size[1] > 2500:
        raise ValueError(f"Size is too large. Must be an integer between 0 and 2500. Actual size is {size}")
    
    evalscript = None
    data_collection = None
    if collection == Collection.Sentinel1:
        evalscript = """
//VERSION=3
function setup() {
    return {
        input: [{
            bands: ["VV", "VH"], 
        }],
        output: %(output)s
    };
}
function evaluatePixel(sample) {
    return {%(evals)s};
}"""
        data_collection = DataCollection.SENTINEL1_IW
    elif collection == Collection.Sentinel2:
        bands = [
            "B02",
            "B03",
            "B04",
            "B08",
            "B11",
            "CLM",
            "CLP",
            "CLD",
            "SCL",
            "sunAzimuthAngles",
            "viewAzimuthMean",
            "sunZenithAngles",
            "viewZenithMean",
        ]
        units = ["DN"] * len(bands)
        evalscript = f"""
//VERSION=3
function setup() {{
   return {{
       input: [{{
           bands: [{", ".join('"' + i + '"' for i in bands)}], 
           units: [{", ".join('"' + i + '"' for i in units)}],
           mosaicking: "SIMPLE",
       }}],
       output: %(output)s
   }};
}}
function evaluatePixel(sample) {{
   return {{%(evals)s}};
}}"""
        data_collection = DataCollection.SENTINEL2_L2A
    elif collection == Collection.DEM:
        evalscript = """
//VERSION=3
function setup() {
  return {
    input: [{
        bands: ["DEM"], 
    }],
    output: %(output)s
  }
}

function evaluatePixel(sample) {
  return {%(evals)s};
}"""
        data_collection = DataCollection.DEM

    outputs = [output_shapes[out_id] for out_id in output_ids]
    evals = ", ".join([evaluate_pixels[out_id] for out_id in output_ids])
    responses = [
        SentinelHubRequest.output_response(out_id, MimeType.TIFF)
        for out_id in output_ids
    ]

    request = SentinelHubRequest(
        evalscript=evalscript % {"output": json.dumps(outputs), "evals": evals},
        input_data=[
            SentinelHubRequest.input_data(
                data_collection=data_collection,
                time_interval=(catalog_date, catalog_date)
                if collection != Collection.DEM
                else None,
            )
        ],
        responses=responses,
        bbox=bbox,
        size=size,
        config=config,
    )
    return request


class DownloadInfo:
    def __init__(self, bbox, start_year, end_year):
        super().__init__()
        self.bbox = bbox
        self.start_year = start_year
        self.end_year = end_year

        self.app_config = ConfigParser()
        self.app_config.read("config.ini")
        self.info = setup_sentinel(self.app_config)

    def download_sentinel_images(self, collection: Collection):
        if collection == Collection.DEM:
            logger.warning(
                "Do not use this method for DEM data, instead use `download_dem_images`"
            )
            return

        counter = 1
        if collection == Collection.Sentinel2:
            list_of_requested_bands = [
                "B02",
                "B03",
                "B04",
                "B08",
                "B11",
                "RGB",
                "CLP",
                "CLD",
                "SCL",
                "sunAzimuthAngles",
                "viewAzimuthMean",
                "sunZenithAngles",
                "viewZenithMean",
            ]
        else:
            list_of_requested_bands = ["VV", "VH"]
        start_date = datetime(year=self.start_year, month=1, day=1).isoformat()
        end_date = datetime(self.end_year, month=12, day=31).isoformat()
        catalog_list = get_available_dates(
            bbox=self.bbox,
            start_date=start_date,
            end_date=end_date,
            collection=collection,
            config=self.info.config,
        )
        root_folder = get_folder_from_bbox(self.bbox, self.app_config)
        for catalog_date in catalog_list:
            request = create_request(
                self.info.config,
                self.bbox,
                self.info.resolution,
                list_of_requested_bands,
                catalog_date,
                collection,
            )
            data_folder = os.path.join(root_folder, catalog_date)
            request.data_folder = data_folder
            request.get_data(save_data=True)
            if not os.path.exists(
                Path(data_folder).joinpath(
                    "B04.tif" if collection == Collection.Sentinel2 else "VV.tif"
                )
            ):
                for root, _, files in os.walk(data_folder):
                    for file in files:
                        if file.endswith("response.tar"):
                            tar_path = os.path.join(root, file)
                            with tarfile.TarFile(tar_path, "r") as tar_ref:
                                tar_ref.extractall(data_folder)

            print(
                f"[{counter}][{datetime.now().time()}] Data fetched for date {catalog_date}"
            )
            counter += 1
            logger.debug(int(counter / len(catalog_list) * 100))
            (
                self.sentinel2_dates
                if collection == "sentinel2"
                else self.sentinel1_dates
            ).append(catalog_date)

    def download_dem_images(self):
        """Download terrain data from sentinel"""

        list_of_requested_bands = ["DEM"]
        root_folder = get_folder_from_bbox(self.bbox, self.app_config)
        request = create_request(
            self.info.config,
            self.bbox,
            self.info.resolution,
            list_of_requested_bands,
            None,
            Collection.DEM,
        )
        request.data_folder = root_folder
        request.get_data(save_data=True)
        dem_name = os.path.join(root_folder, "DEM.tiff")
        if not os.path.exists(dem_name):
            for root, _, files in os.walk(root_folder):
                for file in files:
                    if file.endswith("response.tiff"):
                        os.rename(os.path.join(root, file), dem_name)

        logger.info("DEM Data fetched")

    def download(self):
        logger.warn("This method will likely take a very long time to run")

        self.sentinel1_dates = []
        # logger.debug("Downloading DEM data.")
        # self.download_dem_images()
        # logger.debug("Downloading Sentinel2 Data.")
        # self.download_sentinel_images(Collection.Sentinel2)
        logger.debug("Downloading Sentinel1 Data.")
        self.download_sentinel_images(Collection.Sentinel1)
        logger.info("Done! Sentinel1: %d images." % (len(self.sentinel1_dates)))
