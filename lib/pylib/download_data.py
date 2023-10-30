import sqlite3
from pathlib import Path
import os
import re


def write_download_to_db(folder: Path):
    data = []
    for root, dirs, files in os.walk(folder):
        for dir in dirs:
            if re.match("\d{4}-\d{2}-\d{2}", dir):
                if folder.joinpath(dir).joinpath("B08.tif").exists():
                    year, month, day = [int(x) for x in dir.split("-")]
                    data.append((year, month, day))

    con = sqlite3.connect(folder.joinpath("approximation.db"))
    cur = con.cursor()
    sql = """CREATE TABLE IF NOT EXISTS dates(
year INTEGER NOT NULL,
month INTEGER NOT NULL,
day INTEGER NOT NULL,
clouds_computed INTEGER,
shadows_computed INTEGER,
percent_cloudy REAL,
percent_shadows REAL,
percent_invalid REAL,
PRIMARY KEY(year, month, day));
    """
    cur.execute(sql)

    sql = "INSERT OR IGNORE INTO dates(year, month, day) VALUES(?, ?, ?);"
    cur.executemany(sql, data)
    con.commit()

if __name__ == "__main__":
    write_download_to_db(Path("/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9314176_56.921209032_-111.6817217_57.105787570"))