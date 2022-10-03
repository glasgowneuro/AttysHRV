//
// Created by bp1 on 03/10/2022.
//

#ifndef OCULUSECG_UTIL_H
#define OCULUSECG_UTIL_H

void crossProduct(const float v_A[3], const float v_B[3], float c_P[3]) {
    c_P[0] = v_A[1] * v_B[2] - v_A[2] * v_B[1];
    c_P[1] = -(v_A[0] * v_B[2] - v_A[2] * v_B[0]);
    c_P[2] = v_A[0] * v_B[1] - v_A[1] * v_B[0];
}

#endif //OCULUSECG_UTIL_H
