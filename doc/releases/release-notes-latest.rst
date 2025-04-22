.. _find_my_release_notes_latest:

.. TODO: Change "latest" in above tag to specific version, e.g. 160

.. TODO: Change "from main branch" to specific version, e.g. v1.6.0

Find My add-on for nRF Connect SDK from main branch
###################################################

.. TODO: Remove following note
.. note::
   This file is a work in progress and might not cover all relevant changes.

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

.. TODO: If there are no highlights, remove the section content below and use the following sentence:
         There are no highlights for this release.

This release covers the following features:

There are no entries for this section yet.

.. TODO: Uncomment following section and change version numbers
  Release tag
  ***********

  The release tag for the Find My add-on for nRF Connect SDK repository is **v0.0.0**.
  This release is compatible with nRF Connect SDK **v0.0.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. TODO: Change main to specific version, e.g. v1.6.0
.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr main

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA10095 (nRF5340 Development Kit)
* PCA10156 (nRF54L15 Development Kit)
* PCA10175 (nRF54H20 Development Kit)
* PCA20053 (Thingy:53 Prototyping Platform)

.. TODO: If you adding new kit to this list, add it also to the release-notes-latest.rst.tmpl

Changelog
*********

* Added:

  * Experimental support for the nRF54H20 SoC to the Find My stack that you can enable with the :kconfig:option:`CONFIG_FMNA` Kconfig option.
  * Experimental support for the nRF54H20 DK to the Find My Locator Tag and Pair before use samples.

* Updated:

  * The :c:func:`fmna_sound_cb_register` and :c:func:`fmna_motion_detection_cb_register` functions to allow registering callbacks only when the Find My stack is disabled.
  * The :c:func:`fmna_factory_reset` function to reset the Bluetooth identity used by the Find My stack.
  * The ``Debug`` configuration of Find My samples to use the :kconfig:option:`CONFIG_NCS_SAMPLES_DEFAULTS` Kconfig option.
    The samples now use the minimal logging mode (:kconfig:option:`CONFIG_LOG_MODE_MINIMAL`), which simplifies the logging format and reduces the memory footprint.
    This aligns the logging and assert configuration of Find My samples with the corresponding configurations of most nRF Connect SDK samples.
  * The ``Debug`` configuration of Find My samples to disable RTT logging (:kconfig:option:`CONFIG_USE_SEGGER_RTT`).
    This reduces the memory footprint of the Find My samples.
  * The non-secure target (``nrf5340dk/nrf5340/cpuapp/ns`` and ``thingy53/nrf5340/cpuapp/ns``) configurations of Find My samples and applications to use configurable TF-M profile instead of the predefined minimal TF-M profile.
    This change results from the Bluetooth subsystem transition to the PSA cryptographic standard.
    The Bluetooth stack can now use the PSA crypto API in the non-secure domain as all necessary TF-M partitions are configured properly.
  * The configurations of all Find My samples and applications to enable the Link Time Optimization (:kconfig:option:`CONFIG_LTO` and :kconfig:option:`CONFIG_ISR_TABLES_LOCAL_DECLARATION`).
    This reduces the memory footprint of the Find My samples and applications.

* Removed:

  * The following deprecated API elements from the public Find My header (the :file:`include/fmna.h` file):

    * The ``fmna_enable_param`` and ``fmna_enable_cb`` structures together with the variant of the ``fmna_enable`` function that accepts the removed API structures as its two parameters.
    * The ``fmna_resume`` function.

  * The following Kconfig options from the :file:`src/Kconfig` and :file:`src/uarp/Kconfig` files:

    * ``CONFIG_FMNA_ENABLE_WITH_PARAMETERS``
    * ``CONFIG_FMNA_UARP_PAYLOAD_4CC``
    * ``CONFIG_FMNA_UARP_MCUBOOT_BUF_SIZE``

  * Mentions of the deprecated elements in the Find My stack and its documentation.
  * The use of the ``CONFIG_FMNA_ENABLE_WITH_PARAMETERS`` Kconfig option from the Find My samples and applications configuration.

CLI Tools
=========

* Added:

  * The ``-s/--settings-size`` option to the ``extract`` command in the Find My CLI tools package.
    This option specifies the size of the settings partition and must be used together with the ``-f/--settings-base`` option, which specifies the address of the settings partition.
    By default, the settings partition size is optional when extracting provisioning data from the target device.
    If only the settings partition address is provided, the CLI tools package will scan the NVM from the specified settings partition base to the end of the NVM.
    Use the settings partition size option for the devices that do not define their NVM storage partition at the end of their NVM memory space.
    This way, you can avoid potential memory access restrictions during the NVM readout.
  * Support for the nRF54H20 SoC to the ``provision`` and ``extract`` commands of the Find My CLI tools package.
    With the ``extract`` command, use both the settings partition address (``-f/--settings-base``) and the settings partition size (``-s/--settings-size``) options to avoid potential memory access restrictions during NVM memory reading.

* Updated the CLI Tools version to **v0.7.0**.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the ``Release`` configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* The nRF54L15 SoC current consumption, increased during the NFC tag read operation, does not always return to the initial state after the NFC reader is removed.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.

.. TODO:
  1. Before the release, make sure that all TODO items in the 'release-notes-latest.rst' file are fulfilled and deleted.
  2. Change ending of the 'release-notes-latest.rst' file name to an actual version, e.g. 'release-notes-1.6.0.rst'.
  3. After the release, copy the 'release-notes-latest.rst.tmpl' file to the 'release-notes-latest.rst'.
