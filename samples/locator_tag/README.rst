.. _locator_tag:

Find My Locator tag
###################

Overview
********

The Find My Locator tag sample demonstrates how to use basic features of the Find My stack like Play Sound and Unwanted Tracking (UT) Detection features.

When the application starts, it enables the Find My stack.
To make the accessory discoverable by the Find My iOS application, press **Button 1**.

If the accessory is unpaired, you can pair with it using the mobile application.
Note that the FMN advertising in the Unpaired state times out after 10 minutes.
To trigger the advertising again, press **Button 1** again.

If the accessory is paired, you can track its location using the Find My application.
You can also use the app interface to play a sound on the device.
**LED 1** on the accessory indicates the play sound action.

To reset the accessory to default factory settings, press **Button 4** during the application boot-up.

The sample uses the background device firmware upgrade (DFU) solution called Unified Accessory Restore Protocol (UARP) which is compatible with the Find My iOS application.

Requirements
************

The sample supports the following development kits:

+-------------------+-----------+-------------------------------------+---------+-----------+
|Hardware platforms |PCA        |Board target                         |*ZDebug* |*ZRelease* |
+===================+===========+=====================================+=========+===========+
|nRF54H20 DK        |PCA10175   |``nrf54h20dk/nrf54h20/cpuapp``       | x       | x         |
+-------------------+-----------+-------------------------------------+---------+-----------+
|nRF54L15 DK        |PCA10156   |``nrf54l15dk/nrf54l15/cpuapp``       | x       | x         |
+-------------------+-----------+-------------------------------------+---------+-----------+
|nRF5340 DK         |PCA10095   |``nrf5340dk/nrf5340/cpuapp/ns``      | x       | x         |
|                   |           |``nrf5340dk/nrf5340/cpuapp``         |         |           |
+-------------------+-----------+-------------------------------------+---------+-----------+
|nRF52840 DK        |PCA10056   |``nrf52840dk/nrf52840``              | x       | x         |
+-------------------+-----------+-------------------------------------+---------+-----------+
|nRF52833 DK        |PCA10100   |``nrf52833dk/nrf52833``              |         | x         |
+-------------------+-----------+-------------------------------------+---------+-----------+
|nRF52 DK           |PCA10040   |``nrf52dk/nrf52832``                 |         | x         |
+-------------------+-----------+-------------------------------------+---------+-----------+

User interface
**************

Button 1:
   A short press enables or extends the pairing mode on the accessory.
   A long press (>3 s) activates or deactivates the Find My functionality.

Button 2:
   Enables the Serial Number lookup over Bluetooth Low Energy.

Button 3:
   Simulates the motion detection (used with the UT Detection feature).

Button 4:
   Decrements the battery level.
   Resets the accessory to default factory settings when pressed during the application boot-up or Find My activation.

LED 1:
   Indicates that the play sound action is in progress.

LED 2:
   Indicates that the motion is detected in the current motion detection period.

LED 3:
   LED is blinking to indicate the pairing mode.

   LED is lit to indicate the paired state.

   LED is off to indicate the unpaired state without active pairing mode.

LED 4:
   Indicates that the Find My functionality is enabled.

Qualification mode
******************

The qualification mode exposes an additional Bluetooth GATT characteristic that is used to send test commands.
You can use this sample with enabled qualification mode to execute tests from the Find My Certification Assistant (FMCA) iOS application.

To run Find My stack in the qualification mode enable the :kconfig:option:`CONFIG_FMNA_QUALIFICATION` Kconfig option.

Building and running
********************

To build and run this sample, refer to generic instructions in the :ref:`samples_building` section.

Testing
=======

After provisioning the MFi tokens to your development kit and programming it, complete the following steps to test the sample:

1. Connect to the kit that runs this sample with a terminal emulator (for example, PuTTY).
   See `How to connect with PuTTY for the required settings <https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/test_and_optimize.html>`_.
#. Press **Button 1** to set the Find My accessory in the pairing mode.
#. On your iOS device, pair the accessory using the Find My application:

   a. Open the Find My application.
   #. Navigate to the :guilabel:`Items` tab.
   #. Tap the :guilabel:`Add Item` button.
   #. In the pop-up window, select the :guilabel:`Other Supported Item` option.

#. Observe that iOS starts to search for FMN items.
#. Tap the :guilabel:`Connect` button once the accessory is found.
#. Complete the FMN pairing process.
#. Select the paired accessory from the item list and tap the :guilabel:`Play Sound` button.
#. Observe that **LED 1** is lit for 5 seconds on the accessory to indicate the play sound action.
#. In the Find My application, tap the :guilabel:`Remove Item` button to remove the accessory from the item list.
#. In the pop-up window, tap the :guilabel:`Remove` button to confirm the removal.
#. In the subsequent pop-up window, tap again the :guilabel:`Remove` button to complete the process.
