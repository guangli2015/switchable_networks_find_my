.. _super_on_github:

Create SuperBinary with GitHub Actions
######################################

This sample shows how to use GitHub Actions to compose a SuperBinary remotely on a macOS runner.

The :file:`trigger_action.py` script performs the following steps:

1. Copy the latest commit from the git repository containing the script to a temporary repository.
#. Add input files from the sample directory to the temporary repository.
#. Create and push a new commit to the ``temp-compose-superbinary-now`` branch.
#. Delete the temporary repository.
#. The script stops running on the local machine. 
   It continues to run on GitHub Actions runner with macOS,
   because pushing to ``temp-compose-superbinary-now`` branch triggers the GitHub action from the
   :file:`.github/workflows/build_the_file.yml` file
#. Compose the SuperBinary from the input files.
#. Delete the ``temp-compose-superbinary-now`` branch.
#. Pass the output files as downloadable artifact.

Minutes spent on running GitHub Actions are limited.
Running this script will consume minutes from your quota.
You can check the time spent in your profile settings.

Building
========

To build this sample, complete the following steps:

1. Move the following required files into the script directory:

   * MCUboot application image binary file. 
      You can only have one file.
   * The ``mfigr2`` tool to compose a SuperBinary.

#. Move the following optional files into the script directory:

   * Custom :file:`SuperBinary.plist` file.
   * Custom :file:`MetaData.plist` file.
   * Release notes if you want to also calculate its hash.
     Release notes can be a file or a directory.

#. Make sure that your sdk-find-my github fork is added as remote to your local git repository.
   You can list remote repositories linked with your local git repository using the following command:

   .. code-block:: console

      git remote -v

   If needed, fork the nrfconnect/sdk-find-my repository and then add the fork as a remote repository to your local git repository using the following command:

   .. code-block:: console

      git remote add <remote_name> https://github.com/<user_name>/<repo_name>.git

#. Start the script:

   .. code-block:: console

      python3 tools/samples/SuperBinary/github_action/trigger_action.py

#. Go to GitHub Actions on your remote repository.

#. Wait for the action to complete.

#. Download the artifacts. 
   The artifacts will be available at least for two days.
