.. _find_my_release_notes_280:

Find My add-on for nRF Connect SDK v2.8.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* Added:

  * Support for the nRF54L15 Development Kit.
  * Support for the new storage system called Zephyr Memory Storage (ZMS).

* Removed support for the nRF54L15 Preview Development Kit.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.8.0**.
This release is compatible with nRF Connect SDK **v2.8.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.8.0

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

  * The UARP payload API to simplify the definition of the supported UARP payloads, such as firmware images, and separate their processing from the main UARP integration.
  * The UARP payload defining the MCUboot compatible main application image (:kconfig:option:`CONFIG_FMNA_UARP_PAYLOAD_MCUBOOT_APP`) that supports the MCUboot swap mode.
    This payload is supported by default if the MCUboot bootloader is enabled and configured in the swap mode.
    You can configure the UARP payload identifier (4CC tag) for the MCUboot main application image using the :kconfig:option:`CONFIG_FMNA_UARP_PAYLOAD_MCUBOOT_APP_S0_4CC_TAG` Kconfig option.
  * The Zephyr Memory Storage (ZMS) support in the Find My stack.
    It is enabled by default for devices with the RRAM non-volatile memory that do not require explicit flash erases.
    The nRF54L15 SoC is the only example of such a device that is currently supported in this repository.
  * The support for the nRF54L15 Development Kit (board target ``nrf54l15dk/nrf54l15/cpuapp``).

* Updated:

  * The structure of Find My samples by merging the Find My Simple and Find My Qualification samples into Find My Locator tag.
    See the :ref:`locator_tag` section for more information.
  * Sample name from Find My Coexistence to Find My Pair before use.
    See the :ref:`pair_before_use` section for more information.
  * The Find My Version module to remove the firmware version related dependencies in the :kconfig:option:`CONFIG_FMNA_UARP` Kconfig option.
  * The Find My Serial Number module to return the serial number generated using the Zephyr HWINFO driver when the :kconfig:option:`CONFIG_FMNA_CUSTOM_SERIAL_NUMBER` Kconfig option is not enabled.
    The serial number returned by the HWINFO driver is inverted to maintain the previous behavior of the serial number generation.
    Therefore, the serial number has not changed due to the migration to the HWINFO driver.
    From this perspective, it is safe to update the already deployed products with this release as the serial number remains unchanged after the DFU.
  * The MCUboot partition size for the nRF54L15 DK board target in the ``Release`` configuration of the Find My Locator tag sample to optimize the partition layout of this application.
  * The Find My Network accessory development kit (:kconfig:option:`CONFIG_FMNA`) to imply using a separate workqueue for connection TX notify processing (:kconfig:option:`CONFIG_BT_CONN_TX_NOTIFY_WQ`) if MPSL is used for synchronization between the flash memory driver and radio (:kconfig:option:`CONFIG_SOC_FLASH_NRF_RADIO_SYNC_MPSL`).
    This is done to work around the timeout in MPSL flash synchronization (``NCSDK-29354`` known issue).
    See `nRF Connect SDK known issues <https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/releases_and_maturity/known_issues.html>`_ for details.

* Deprecated:

  * The :kconfig:option:`CONFIG_FMNA_UARP_PAYLOAD_4CC` Kconfig option that controlled the UARP payload 4CC tag.
    Use the MCUboot payload specific :kconfig:option:`CONFIG_FMNA_UARP_PAYLOAD_MCUBOOT_APP_S0_4CC_TAG` Kconfig option instead.
  * The :kconfig:option:`CONFIG_FMNA_UARP_MCUBOOT_BUF_SIZE` Kconfig option that controlled the buffer size needed for flash writes to MCUboot slot.
    Use the MCUboot payload writer specific :kconfig:option:`CONFIG_FMNA_UARP_WRITER_MCUBOOT_BUF_SIZE` Kconfig option instead.

* Removed:

  * The common configuration files from the :file:`samples/common` directory.
    The MCUboot bootloader image configuration and Partition Manager files are placed in the sample-specific directories.
  * The support for the nRF54L15 Preview Development Kit (board target ``nrf54l15pdk/nrf54l15/cpuapp``).

CLI Tools
=========

* Added:

  * An error notification in the :ref:`super_on_github` sample for cases where a remote GitHub repository is not connected.
  * The Zephyr Memory Storage (ZMS) support in the ``provision`` and ``extract`` commands of the Find My CLI tools package.
    It is enabled by default for the nRF54L15 SoC.
    You can overwrite the default configuration using the ``-n/--nv-storage`` option.

* Updated:

  * The required Python version for the CLI Tools to **3.12** or newer.
  * The CLI Tools version to **v0.6.0**.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the ``Release`` configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* The nRF54L15 SoC current consumption, increased during the NFC tag read operation, does not always return to the initial state after the NFC reader is removed.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
