param(
[string]$arg1
)

docker run --rm -ti  -v "$(pwd):/app:rw" --privileged -v "/dev/bus/usb:/dev/bus/usb" -e DISPLAY=10.0.55.135:0 app-tester:latest sh -c "cd app && pytest -v --display --tb=short tests/ --cov ragger --cov-report xml --device $arg1"