import logging

def setup_logger(filename: str | None = None, level: int =logging.DEBUG) -> None:
    fmt = '%(asctime)s | [%(threadName)s] | %(levelname)s | %(filename)s:%(lineno)d | %(funcName)s() | %(message)s'

    if filename:
        logging.basicConfig(
            level=level,
            format=fmt,
            filename=filename,
            filemode='a'
        )
    else:
        logging.basicConfig(
            level=level,
            format=fmt
        )
