sudo bash ../scripts/build.sh SP
pytest --tb=short -v tests/ --device nanosp
sudo bash ../scripts/build.sh S
pytest --tb=short -v tests/ --device nanos
sudo bash ../scripts/build.sh X
pytest --tb=short -v tests/ --device nanox
sudo bash ../scripts/build.sh STAX
pytest --tb=short -v tests/ --device stax

