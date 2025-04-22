.. _samples:

Samples
#######

.. contents::
   :local:
   :depth: 2

The Find My add-on provides a set of samples that demonstrate how to work with the Find My stack.

Find My samples
===============

   .. toctree::
      :maxdepth: 1
      :glob:

      ../samples/*/README

Build configurations
====================

You can build each Find My sample in the following configurations:

- The ``Release`` configuration called *ZRelease*
- The ``Debug`` configuration called *ZDebug*

To select the build configuration, populate the ``FILE_SUFFIX`` variable in the build process.
For example:

.. code-block:: console

   west build -b nrf52dk/nrf52832 -- -DFILE_SUFFIX=ZRelease

The *ZDebug* build configuration is used by default if the ``FILE_SUFFIX`` variable is not specified

Your build command displays the information about the selected build configuration at the beginning of its log output.
For the ``Release`` configuration, it would look like this:

.. code-block:: console

   -- Build type: ZRelease

.. note::
   The ``Release`` configuration disables logging in the sample.

To run the Find My stack in the qualification mode, use the :kconfig:option:`CONFIG_FMNA_QUALIFICATION` Kconfig option.
For example:

.. code-block:: console

   west build -b nrf52840dk/nrf52840 -- -DCONFIG_FMNA_QUALIFICATION=y

.. _samples_building:

Building and running a sample
=============================

.. note::
   Configure your MFi product plan in a Find My sample before building and running it.
   Then, provision your development kit with a HEX file containing the MFi tokens that belong to your plan.
   See :ref:`find_my_pairing` for more details.

To build a Find My sample, complete the following steps:

1. Go to the Find My sample directory:

   .. code-block:: console

      cd <ncs_path>/find-my/samples/<sample_type>

#. Build the sample using west.

   For the ``Debug`` configuration, use the following command:

   .. code-block:: console

      west build -b nrf52840dk/nrf52840

   For the ``Release`` configuration, use the following command:

   .. code-block:: console

      west build -b nrf52840dk/nrf52840 -- -DFILE_SUFFIX=ZRelease

#. Connect the development kit to your PC using a USB cable and program the sample or application to it using the following command:

   .. code-block:: console

      west flash

   To fully erase the development kit before programming the new sample or application, use the command:

   .. code-block:: console

      west flash --erase

For more information on building and programming using the command line, see the `Zephyr documentation on Building, Flashing, and Debugging <https://docs.nordicsemi.com/bundle/ncs-latest/page/zephyr/develop/west/build-flash-debug.html>`_.
