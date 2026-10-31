.PHONY: build test format format-check check preview fixtures
build:
	pio run -e esp32s3 -e diagnostic -e fixture -e hub75-diagnostic -e hub75-fixture -e hub75-badge-contrast -e hub75-live -e hub75-typography
test:
	pio test -e native
	python tools/test_device_identity.py
format:
	python tools/format.py
format-check:
	python tools/format.py --check
fixtures:
	python tools/generate_fixtures.py
check: format-check
	python tools/generate_fixtures.py --check
	python tools/test_device_identity.py
	pio test -e native
	pio run -e esp32s3 -e diagnostic -e fixture -e hub75-diagnostic -e hub75-fixture -e hub75-badge-contrast -e hub75-live -e hub75-typography
preview:
	python tools/preview.py
