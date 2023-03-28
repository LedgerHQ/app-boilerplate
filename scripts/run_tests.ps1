param(
[string]$arg1
)

$pkg_dependencies="apk add gcc musl-dev && apk add python3-dev && pip install pysha3"

# with display
docker run --rm -ti  -v "$(pwd):/app:rw" --privileged -v "/dev/bus/usb:/dev/bus/usb" -e DISPLAY=10.0.55.135:0 app-tester:latest sh -c "$pkg_dependencies && cd app && pytest -v --display --tb=short tests/ --cov ragger --cov-report xml --device $arg1"

# without display
# docker run --rm -ti  -v "$(pwd):/app:rw" --privileged -v "/dev/bus/usb:/dev/bus/usb" -e DISPLAY=10.0.55.135:0 app-tester:latest sh -c "$pkg_dependencies && cd app && pytest -v --tb=short tests/ --cov ragger --cov-report xml --device $arg1"




