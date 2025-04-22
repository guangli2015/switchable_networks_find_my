.. _find_my_release_notes_270:

Find My add-on for nRF Connect SDK v2.7.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* Added experimental support for the nRF54L15 device.
* Migrated to the new board name scheme that was introduced in the nRF Connect SDK following the changes in Zephyr.
* Migrated all Find My samples and the Find My Thingy application from multi-image builds (child/parent image) to Zephyr's System Build (sysbuild).

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.7.0**.
This release is compatible with nRF Connect SDK **v2.7.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.7.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA10095 (nRF5340 Development Kit)
* PCA10156 (nRF54L15 Preview Development Kit)
* PCA20053 (Thingy:53 Prototyping Platform)

Changelog
*********

* Added experimental support for the nRF54L15 SoC to the Find My stack module that you can enable with the :kconfig:option:`CONFIG_FMNA` Kconfig option.
* Added experimental support for the nRF54L15 PDK to the Find My Simple, Qualification and Coexistence samples.
  Samples are supported in the ``Debug`` and ``Release`` configurations.
* Added the UARP payload writer API to separate the payload-specific processing logic from the UARP integration, making it easier to add new types of payloads.
* Changed logging backend from UART to USB CDC ACM for the Find My Thingy application in the ``Debug`` configuration.
* The samples and applications that used the Bluetooth HCI IPC sample from ``sdk-zephyr`` as a network core image for board targets with nRF5340 SoC, now use the IPC Radio Firmware application from ``sdk-nrf``.
* Migrated to the new board name scheme that was introduced in the nRF Connect SDK following the changes in Zephyr.
  For more information about the new board name scheme (referred to as “hardware model v2”), see the `Board Porting Guide <https://docs.nordicsemi.com/bundle/ncs-latest/page/zephyr/hardware/porting/board_porting.html>`_ section in the Zephyr documentation.
* Migrated all Find My samples and the Find My Thingy application from multi-image builds (child/parent image) to Zephyr's System Build (sysbuild).
  In the nRF Connect SDK, building with sysbuild is now enabled by default.
  For more information about the sysbuild, see the `Sysbuild (System build) <https://docs.nordicsemi.com/bundle/ncs-latest/page/zephyr/build/sysbuild/index.html>`_ section in the Zephyr documentation.
* Changed the selection process for the build configuration.
  Instead of populating the ``CMAKE_BUILD_TYPE`` variable during the build process, use the ``FILE_SUFFIX``.
  For more information, see the :ref:`samples_building` section.

CLI Tools
=========

* Added support for the nRF54L15 SoC to the ``provision`` and ``extract`` commands of the Find My CLI tools package.
* Changed the minimal required version of the CLI Tools dependency ``pynrfjprog`` Python package to **10.23.5**.
* Upgraded the CLI Tools version to **v0.5.0**.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the ``Release`` configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
