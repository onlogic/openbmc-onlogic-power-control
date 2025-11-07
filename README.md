# OpenBMC OnLogic Power Control

This application is designed to interface with OnLogic systems to provide Host and Chassis power control to the OpenBMC control plane.

## Getting Started

1. Install Boost and Meson via your OS package provider
1. Create a python venv `python -m venv .venv`
1. Install python requirements `pip install -r requirements.txt`
1. Setup Meson build environment `meson setup builddir`
1. Build the project `cd builddir && ninja`