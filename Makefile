APP_NAME=lcd-server
INSTALL_DIR=/opt/$(APP_NAME)
SERVICE_FILE=$(APP_NAME).service
PYTHON=python3
VENV=$(INSTALL_DIR)/venv
USER?=

all: help

help:
	@echo "Available targets:"
	@echo "  make install       -> Install script, dependencies, and service"
	@echo "  make start         -> Start the systemd service"
	@echo "  make stop          -> Stop the systemd service"
	@echo "  make restart       -> Restart the service"
	@echo "  make logs          -> Show service logs"
	@echo "  make test          -> Send a test message to the LCD"
	@echo "  make uninstall     -> Remove the service and installation"
	@echo ""
	@echo "Optional: pass USER=yourusername to run service as a specific user"

install:
	@echo "[+] Creating install dir at $(INSTALL_DIR)"
	mkdir -p $(INSTALL_DIR)
	cp lcd_tcp_server.py requirements.txt $(INSTALL_DIR)/

	@echo "[+] Setting up Python virtual environment"
	cd $(INSTALL_DIR) && $(PYTHON) -m venv venv && . venv/bin/activate && venv/bin/pip install -r requirements.txt

	@echo "[+] Creating systemd service"
	@echo "    USER: $(USER)"
	sed "s|/usr/bin/python3|$(INSTALL_DIR)/venv/bin/python|g" $(SERVICE_FILE) | \
	sed "s|/opt/lcd-server|$(INSTALL_DIR)|g" | \
	( if [ -n "$(USER)" ]; then sed "s|^#User=.*|User=$(USER)|"; else cat; fi ) | \
	tee /etc/systemd/system/$(APP_NAME).service > /dev/null

	@echo "[+] Enabling and reloading systemd"
	systemctl daemon-reexec
	systemctl daemon-reload
	systemctl enable $(APP_NAME)

	@echo "[✔] Installation complete. Use 'make start' to run the service."

start:
	systemctl start $(APP_NAME)
	@echo "[✔] Service started."

stop:
	systemctl stop $(APP_NAME)
	@echo "[✔] Service stopped."

restart:
	systemctl restart $(APP_NAME)
	@echo "[✔] Service restarted."

logs:
	journalctl -fu $(APP_NAME)

test:
	@echo "[TEST] Sending test message to LCD..."
	echo "x=0&y=0&message=LCD is alive!" | nc 127.0.0.1 9999

uninstall:
	@echo "[!] Removing service and installation..."
	systemctl stop $(APP_NAME)
	systemctl disable $(APP_NAME)
	rm -f /etc/systemd/system/$(APP_NAME).service
	rm -rf $(INSTALL_DIR)
	systemctl daemon-reload
	@echo "[✔] Service and files removed."
