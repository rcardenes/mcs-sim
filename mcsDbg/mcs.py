# vim: ai:sw=4:sts=4:expandtab

from collections import namedtuple
import _mcs

az_jump = 0.1
el_jump = 0.1

##################################################################
# MCS Internal parameters (_mcs.McsParams)
#
# This structure is used to keep initialization data
# for parameters that need to be persisted across
# calls to the simulated functions
#
#   firstFitXX   - True if the next fit will be the first on
#
#   azA..C       - Parameters saved from the previous iteration
#   elA..C       - Parameters saved from the previous iteration
#   lastXXVelocity
#                - Last of the extrapolated velocities for Az/El
#                  for the most recent calculation
#   prevXXDemand - Demand calculated in the previous iteration
#   prevXXVel    - Velocity calculated in the previous iteration

# All values for Demand are doubles
Demand    = namedtuple('Demand', "applyTime az el")

class McsCalcSimulator(object):
    def __init__(self):
        self.params = _mcs.McsParams()

    def extrapolate(self, prevDemands):
        """
        Function that calls the MCS follow "fillBuffer" function to extrapolate
        future PMAC demands based on the internal state and inputs:

          prevDemands: sequence of exactly 3 Demand objects, from the previous
                       iteration
        """

        # Extrapolate demands for Azimuth
        ret = _mcs.fillBuffer(
                        self.params,
                        [(dem.applyTime, dem.az) for dem in prevDemands],
                        axis = 1
                        offset = offset,
                        jump = az_jump,
                        max_vel = azCurrentMaxVel,
                        max_acc = azCurrentMaxAcc,
                        curr_pos = azCurrentPos,
                        curr_vel = azCurrentVel)

        # Extrapolate demands for Elevation
        ret = _mcs.fillBuffer(
                        self.params,
                        [(dem.applyTime, dem.al) for dem in prevDemands],
                        axis = 2
                        offset = offset,
                        jump = el_jump,
                        max_vel = elCurrentMaxVel,
                        max_acc = elCurrentMaxAcc,
                        curr_pos = elCurrentPos,
                        curr_vel = elCurrentVel)
