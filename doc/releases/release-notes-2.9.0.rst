.. _find_my_release_notes_290:

Find My add-on for nRF Connect SDK v2.9.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* Changed the Find My API to improve the stack lifecycle management.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.9.0**.
This release is compatible with nRF Connect SDK **v2.9.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.9.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA10095 (nRF5340 Development Kit)
* PCA10156 (nRF54L15 Development Kit)
* PCA20053 (Thingy:53 Prototyping Platform)

Changelog
*********

* Added:

  * The :kconfig:option:`CONFIG_FMNA_ENABLE_WITH_PARAMETERS` Kconfig option to deprecate the current version of the :c:func:`fmna_enable` function that takes two additional parameters.
    This Kconfig option is enabled by default to maintain backwards compatibility of the Find My API for the current nRF Connect SDK release.
    The deprecated Find My API will be removed in the future nRF Connect SDK release together with the :kconfig:option:`CONFIG_FMNA_ENABLE_WITH_PARAMETERS` Kconfig option.
    When this option is disabled, the Find My stack enabling process is split into multiple API functions.
    This approach allows you to separately call the following functions:

    * :c:func:`fmna_id_set` to set the Find My Bluetooth identity.
    * :c:func:`fmna_info_cb_register` to register the Find My information callbacks.
    * :c:func:`fmna_factory_reset` to perform the factory reset.
    * :c:func:`fmna_battery_level_set` to set the initial battery level.
    * :c:func:`fmna_enable` to enable the Find My stack.

    This configuration is recommended for new projects.
    See the API reference documentation for more details.
  * The :c:func:`fmna_serial_number_lookup_cb_register` function to register callbacks defined by the :c:struct:`fmna_serial_number_lookup_cb` structure used to handle the activities related to serial number lookup.
    See the API reference documentation for more details.

* Updated:

  * The Find My samples and applications to disable the :kconfig:option:`CONFIG_FMNA_ENABLE_WITH_PARAMETERS` Kconfig option and use the new API functions.
  * The MCUboot bootloader configurations for the ``nrf54l15dk/nrf54l15/cpuapp`` board target in the Find My Locator Tag sample to enable the :kconfig:option:`CONFIG_FPROTECT` Kconfig option that is used to protect the bootloader partition against memory corruption.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the ``Release`` configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* The nRF54L15 SoC current consumption, increased during the NFC tag read operation, does not always return to the initial state after the NFC reader is removed.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
