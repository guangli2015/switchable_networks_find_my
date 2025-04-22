.. _find_my_release_notes_2699_cs1:

Find My add-on for nRF Connect SDK v2.6.99-cs1
##############################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

nRF Connect SDK v2.6.99-cs1 is a **customer sampling** release, tailored exclusively for participants in the nRF54L15 customer sampling program.
Do not use this release of nRF Connect SDK if you are not participating in the program.

The release is not part of the regular release cycle and is specifically designed to support early adopters working with the nRF54L15 device.
It is intended to provide early access to the latest developments for nRF54L15, enabling participants to experiment with new features and provide feedback.

The scope of this release is intentionally narrow, concentrating solely on the nRF54L15 device.
You can use the latest stable release of nRF Connect SDK to work with other boards.

All functionality related to nRF54L15 is considered experimental.

Highlights
**********

This release covers the following features:

* Added experimental support for the nRF54L15 device.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.6.99-cs1**.
This release is compatible with nRF Connect SDK **v2.6.99-cs1** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.6.99-cs1

Supported development kits
**************************

* PCA10156 (nRF54L15 Preview Development Kit)

Changelog
*********

* Added experimental support for the nRF54L15 SoC to the Find My stack module that you can enable with the :kconfig:option:`CONFIG_FMNA` Kconfig option.
* Added experimental support for the nRF54L15 PDK to the Find My Simple, Qualification and Coexistence samples.
  Samples are supported in the ``Debug`` and ``Release`` configurations.

CLI Tools
=========

* Added support for the nRF54L15 SoC to the ``provision`` and ``extract`` commands of the Find My CLI tools package.
* Changed the minimal required version of the CLI Tools dependency ``pynrfjprog`` Python package to **10.23.5**.
* Upgraded the CLI Tools version to **v0.5.0**.

Known issues and limitations
****************************

* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.

Known issues and limitations for the nRF54L15 PDK
*************************************************

* The v0.2.1 revision of the nRF54L15 PDK has **Button 3** and **Button 4** connected to a GPIO port that does not support interrupts (GPIO port 2).

  **Workaround:** The :kconfig:option:`CONFIG_DK_LIBRARY_BUTTON_NO_ISR` Kconfig option is enabled by default in all supported samples, which makes the **Button 3** and **Button 4** functional on the nRF54L15 PDK.
  The DK Buttons and LEDs library, with the :kconfig:option:`CONFIG_DK_LIBRARY_BUTTON_NO_ISR` Kconfig option enabled, polls the PDK button state periodically (50 ms by default) and reports its status according to the poll results.
  Using the :kconfig:option:`CONFIG_DK_LIBRARY_BUTTON_NO_ISR` Kconfig option increases the overall power consumption of the system.
  When measuring power consumption, disable this option.
