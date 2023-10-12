
# --------------------------------------------------------- SERIAL
# DEFAULT serial parameters: SERIAL_ARDUINO class will be initialized with these values if not
serial_default_port = "/dev/ttyACM0"
serial_default_baud = 500000  # 115200
serial_default_timeout = 1  # in seconds
serial_default_delay_after_setup = 1  # in seconds
ARDUINO_READY_FLAG = "READY"
DEFAULT_SERIAL_ELAPSED = 0.02  # in seconds

# --------------------------------------------------------- NETWORKING

DEFAULT_ESP_PORT = 4210

# Listen on Port: DEFAULT PORT of the socket connection of the raspberry
default_robot_port = 44444
# Size of receive buffer
default_buffer_size = 1024

DEFAULT_NETWORK_SEND_ELAPSED_SEC = 0.02  # in seconds
DEFAULT_MAX_CONSECUTIVE_MSG_READS = 3

DELIMITER = ":"
MSG_DELIMITER = '_'

# --------------------------------------------------------- CONTROL

min_control_value = 0
max_control_value = 255

