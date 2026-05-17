[Unit]
Description=Boot hello unit for CrisOS
WantedBy=boot.target

[Service]
ExecStart=echo Boot completed successfully
