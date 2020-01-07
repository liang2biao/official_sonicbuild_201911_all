#
# component_base.py
#
# Abstract base class for implementing a platform-specific class
# to interact with a chassis/module component (e.g., BIOS, CPLD, FPGA, etc.) in SONiC
#


class ComponentBase(object):
    """
    Abstract base class for implementing a platform-specific class
    to interact with a chassis/module component (e.g., BIOS, CPLD, FPGA, etc.)
    """

    def get_name(self):
        """
        Retrieves the name of the component

        Returns:
            A string containing the name of the component
        """
        raise NotImplementedError

    def get_description(self):
        """
        Retrieves the description of the component

        Returns:
            A string containing the description of the component
        """
        raise NotImplementedError

    def get_firmware_version(self):
        """
        Retrieves the firmware version of the component

        Returns:
            A string containing the firmware version of the component
        """
        raise NotImplementedError

    def install_firmware(self, image_path):
        """
        Installs firmware to the component

        Args:
            image_path: A string, path to firmware image

        Returns:
            A boolean, True if install was successful, False if not
        """
        raise NotImplementedError
