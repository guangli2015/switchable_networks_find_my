.. _find_my_release_notes_260:

Find My add-on for nRF Connect SDK v2.6.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

There are no highlights for this release.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.6.0**.
This release is compatible with nRF Connect SDK **v2.6.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.6.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA10095 (nRF5340 Development Kit)
* PCA20053 (Thingy:53 Prototyping Platform)

Changelog
*********

* Updated the connection module to disconnect the Bluetooth LE connection associated with the Find My functionality if the link encryption is not completed within the predefined time.
  The :kconfig:option:`CONFIG_FMNA_CONN_SECURITY_TIMEOUT` Kconfig option controls the timeout value in seconds and is by default set to 10 seconds to respect specification requirements.
* Updated the connection module to disconnect the Bluetooth LE connection associated with the Find My functionality on the link encryption failure.
* Switched to using minimal C library (:kconfig:option:`CONFIG_MINIMAL_LIBC`) instead of picolibc (:kconfig:option:`CONFIG_PICOLIBC`) in MCUboot configuration of Find My samples.
  This reduces the flash usage of the bootloader significantly.

CLI Tools
=========

* The ``extract`` and ``provision`` commands now take into account differences in the write block size parameter of the target device.
* Refactored and reorganized the :file:`settings_nvs_utils.py` file that contains the shared base module for the ``extract`` and ``provision`` command modules.
* If the superbinary command is executed with incorrect parameters, the CLI usage documentation is displayed.
* Upgraded the CLI Tools version to **v0.4.0**.
* Removed the CLI Tools dependency on the ``six`` Python package.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the ``Release`` configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
