#ifndef MPC5746RECC_H
#define MPC5746RECC_H

#include <cstdint>
#include <QByteArray>

class MPC5746RECC
{
public:
    MPC5746RECC();

    static int32_t encodeEcc_address(int32_t addr);
    static int32_t encodeEcc32_upper (int32_t data_a2_is_zero);
    static int32_t encodeEcc32_lower (int32_t data_a2_is_one);
    static int32_t encodeEcc64(int32_t addr, int64_t flash_array);
    static int32_t encodeEcc64(QByteArray addr, QByteArray flash_array);

    template <typename T>
    static T QByteArray_toInt(QByteArray array);
};

#endif // MPC5746RECC_H
