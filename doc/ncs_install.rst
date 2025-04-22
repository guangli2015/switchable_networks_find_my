.. _ncs_install:

Installing the nRF Connect SDK
##############################

Use the west tool to install all components of the nRF Connect SDK, including the Find My add-on.
The required version of west is 0.10.0 or higher.

1. Follow the installation instructions in the `nRF Connect SDK Installation guide <https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation.html>`_.
#. Enable the Find My group filter:

   .. code-block:: console

      west config manifest.group-filter +find-my

   .. note::
      For more information about how west groups work, see `west groups documentation <https://docs.nordicsemi.com/bundle/ncs-latest/page/zephyr/develop/west/manifest.html#project_groups>`_.

#. Update west to download the Find My repository:

   .. code-block:: console

      west update
      // find-my repository is downloaded
      // your github access will be checked

#. Navigate to the Find My tools directory:

   .. code-block:: console

      cd <ncs_path>/find-my/tools

   Follow the installation section of the :ref:`cli_tools` to install a set of tools that help with the creation of Find My firmware.
