[Unit]
Description=Boot hello unit for NucleOS
WantedBy=boot.target

[Service]
ExecStart=echo Boot completed successfully
