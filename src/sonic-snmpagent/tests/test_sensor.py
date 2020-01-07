import os
import sys

modules_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(modules_path, 'src'))

from unittest import TestCase

# noinspection PyUnresolvedReferences
import tests.mock_tables.dbconnector

from ax_interface.mib import MIBTable
from ax_interface.pdu import PDUHeader
from ax_interface.pdu_implementations import GetPDU, GetNextPDU
from ax_interface import ValueType
from ax_interface.encodings import ObjectIdentifier
from ax_interface.constants import PduTypes
from sonic_ax_impl.mibs.ietf import rfc3433
from sonic_ax_impl.main import SonicMIB

class TestSonicMIB(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lut = MIBTable(rfc3433.PhysicalSensorTableMIB)
        cls.XCVR_SUB_ID = 1 * 1000
        cls.XCVR_CHANNELS = (1, 2, 3, 4)

        # Update MIBs
        for updater in cls.lut.updater_instances:
            updater.reinit_data()
            updater.update_data()

    @staticmethod
    def generate_oids_for_physical_sensor_mib(sub_id):

        return [ObjectIdentifier(12, 0, 0, 0, (1, 3, 6, 1, 2, 1, 99, 1, 1, 1, i, sub_id))
                for i in range(1, 5)]

    def _test_getpdu_xcvr_sensor(self, sub_id, expected_values):
        """
        Test case for correctness of transceiver sensor MIB values
        :param sub_id: sub OID of the sensor
        :expected values: iterable of expected values TYPE, SCALE, PRECISION, VALUE
        """

        # generate OIDs for TYPE(.1), SCALE(.2), PRECISION(.3), VALUE(.4)
        # for given sub OID
        oids = self.generate_oids_for_physical_sensor_mib(sub_id)

        get_pdu = GetPDU(
            header=PDUHeader(1, PduTypes.GET, 16, 0, 42, 0, 0, 0),
            oids=oids
        )

        encoded = get_pdu.encode()
        response = get_pdu.make_response(self.lut)

        for index, value in enumerate(response.values):
            self.assertEqual(str(value.name), str(oids[index]))
            self.assertEqual(value.type_, ValueType.INTEGER)
            self.assertEqual(value.data, expected_values[index])


    def test_getpdu_xcvr_temperature_sensor(self):
        """
        Test case for correct transceiver temperature sensor MIB values
        """

        expected_values = [
            rfc3433.EntitySensorDataType.CELSIUS,
            rfc3433.EntitySensorDataScale.UNITS,
            6, # precision
            25390000, # expected sensor value
            rfc3433.EntitySensorStatus.OK
        ]

        self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 1, expected_values)

    def test_getpdu_xcvr_voltage_sensor(self):
        """
        Test case for correct transceiver voltage sensor MIB values
        """

        expected_values = [
            rfc3433.EntitySensorDataType.VOLTS_DC,
            rfc3433.EntitySensorDataScale.UNITS,
            4, # precision
            33700, # expected sensor value
            rfc3433.EntitySensorStatus.OK
        ]

        self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 2, expected_values)

    def test_getpdu_xcvr_rx_power_sensor_minus_infinity(self):
        """
        Test case for correct transceiver rx power sensor MIB values
        in case when rx power == -inf
        """

        expected_values = [
            rfc3433.EntitySensorDataType.WATTS,
            rfc3433.EntitySensorDataScale.MILLI,
            4, # precision
            0, # expected sensor value
            rfc3433.EntitySensorStatus.OK
        ]

        channel = 1
        self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 10 * channel + 1, expected_values)

    def test_getpdu_xcvr_rx_power_sensor(self):
        """
        Test case for correct transceiver rx power sensor MIB values
        """

        expected_values = [
            rfc3433.EntitySensorDataType.WATTS,
            rfc3433.EntitySensorDataScale.MILLI,
            4, # precision
            7998, # expected sensor value
            rfc3433.EntitySensorStatus.OK
        ]

        # test for each channel except first, we already test above
        for channel in (2, 3, 4):
            self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 10 * channel + 1, expected_values)

    def test_getpdu_xcvr_tx_power_sensor(self):
        """
        Test case for correct transceiver rx power sensor MIB values
        """

        expected_values = [
            rfc3433.EntitySensorDataType.WATTS,
            rfc3433.EntitySensorDataScale.MILLI,
            4, # precision
            2884, # expected sensor value
            rfc3433.EntitySensorStatus.OK
        ]

        # test for each channel except first, we already test above
        for channel in (1, 2, 3, 4):
            self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 10 * channel + 3, expected_values)

    def test_getpdu_xcvr_tx_bias_sensor_unknown(self):
        """
        Test case for correct transceiver tx bias sensor MIB values, when
        tx bias sensor is set to "UNKNOWN" in state DB
        """

        expected_values = [
            rfc3433.EntitySensorDataType.AMPERES,
            rfc3433.EntitySensorDataScale.MILLI,
            3, # precision
            0, # expected sensor value
            rfc3433.EntitySensorStatus.UNAVAILABLE
        ]

        channel = 1
        self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 10 * channel + 2, expected_values)

    def test_getpdu_xcvr_tx_bias_sensor_overflow(self):
        """
        Test case for correct transceiver tx bias sensor MIB values
        when tx bias is grater than 1E9
        """

        expected_values = [
            rfc3433.EntitySensorDataType.AMPERES,
            rfc3433.EntitySensorDataScale.MILLI,
            3, # precision
            1E9, # expected sensor value
            rfc3433.EntitySensorStatus.OK
        ]

        channel = 3
        self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 10 * channel + 2, expected_values)

    def test_getpdu_xcvr_tx_bias_sensor(self):
        """
        Test case for correct transceiver tx bias sensor MIB values
        """

        expected_values = [
            rfc3433.EntitySensorDataType.AMPERES,
            rfc3433.EntitySensorDataScale.MILLI,
            3, # precision
            4440, # expected sensor value
            rfc3433.EntitySensorStatus.OK
        ]

        # test for each channel
        for channel in (2, 4):
            self._test_getpdu_xcvr_sensor(self.XCVR_SUB_ID + 10 * channel + 2, expected_values)

