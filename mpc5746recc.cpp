#include "mpc5746recc.h"
#include <QDebug>

MPC5746RECC::MPC5746RECC() {}

int32_t MPC5746RECC::encodeEcc_address(int32_t addr) {
    int32_t addr_ecc
        =   (((addr            >> 31) & 0x1) == 0x1 ? 0x1f : 0x0)      /* addr[31] */
          ^ (((addr            >> 30) & 0x1) == 0x1 ? 0xf4 : 0x0)      /* addr[30] */
          ^ (((addr            >> 29) & 0x1) == 0x1 ? 0x3b : 0x0)      /* addr[29] */
          ^ (((addr            >> 28) & 0x1) == 0x1 ? 0xe3 : 0x0)      /* addr[28] */
          ^ (((addr            >> 27) & 0x1) == 0x1 ? 0x5d : 0x0)      /* addr[27] */
          ^ (((addr            >> 26) & 0x1) == 0x1 ? 0xda : 0x0)      /* addr[26] */
          ^ (((addr            >> 25) & 0x1) == 0x1 ? 0x6e : 0x0)      /* addr[25] */
          ^ (((addr            >> 24) & 0x1) == 0x1 ? 0xb5 : 0x0)      /* addr[24] */
          ^ (((addr            >> 23) & 0x1) == 0x1 ? 0x8f : 0x0)      /* addr[23] */
          ^ (((addr            >> 22) & 0x1) == 0x1 ? 0xd6 : 0x0)      /* addr[22] */
          ^ (((addr            >> 21) & 0x1) == 0x1 ? 0x79 : 0x0)      /* addr[21] */
          ^ (((addr            >> 20) & 0x1) == 0x1 ? 0xba : 0x0)      /* addr[20] */
          ^ (((addr            >> 19) & 0x1) == 0x1 ? 0x9b : 0x0)      /* addr[19] */
          ^ (((addr            >> 18) & 0x1) == 0x1 ? 0xe5 : 0x0)      /* addr[18] */
          ^ (((addr            >> 17) & 0x1) == 0x1 ? 0x57 : 0x0)      /* addr[17] */
          ^ (((addr            >> 16) & 0x1) == 0x1 ? 0xec : 0x0)      /* addr[16] */
          ^ (((addr            >> 15) & 0x1) == 0x1 ? 0xc7 : 0x0)      /* addr[15] */
          ^ (((addr            >> 14) & 0x1) == 0x1 ? 0xae : 0x0)      /* addr[14] */
          ^ (((addr            >> 13) & 0x1) == 0x1 ? 0x67 : 0x0)      /* addr[13] */
          ^ (((addr            >> 12) & 0x1) == 0x1 ? 0x9d : 0x0)      /* addr[12] */
          ^ (((addr            >> 11) & 0x1) == 0x1 ? 0x5b : 0x0)      /* addr[11] */
          ^ (((addr            >> 10) & 0x1) == 0x1 ? 0xe6 : 0x0)      /* addr[10] */
          ^ (((addr            >>  9) & 0x1) == 0x1 ? 0x3e : 0x0)      /* addr[ 9] */
          ^ (((addr            >>  8) & 0x1) == 0x1 ? 0xf1 : 0x0)      /* addr[ 8] */
          ^ (((addr            >>  7) & 0x1) == 0x1 ? 0xdc : 0x0)      /* addr[ 7] */
          ^ (((addr            >>  6) & 0x1) == 0x1 ? 0xe9 : 0x0)      /* addr[ 6] */
          ^ (((addr            >>  5) & 0x1) == 0x1 ? 0x3d : 0x0)      /* addr[ 5] */
          ^ (((addr            >>  4) & 0x1) == 0x1 ? 0xf2 : 0x0)      /* addr[ 4] */
          ^ (((addr            >>  3) & 0x1) == 0x1 ? 0x2f : 0x0);     /* addr[ 3] */

    return addr_ecc;
}

int32_t MPC5746RECC::encodeEcc32_upper (int32_t data_a2_is_zero)
{
    int32_t ecc
        =   (((data_a2_is_zero >> 31) & 0x1) == 0x1 ? 0xb0 : 0x0)      /* data[63] */
          ^ (((data_a2_is_zero >> 30) & 0x1) == 0x1 ? 0x23 : 0x0)      /* data[62] */
          ^ (((data_a2_is_zero >> 29) & 0x1) == 0x1 ? 0x70 : 0x0)      /* data[61] */
          ^ (((data_a2_is_zero >> 28) & 0x1) == 0x1 ? 0x62 : 0x0)      /* data[60] */
          ^ (((data_a2_is_zero >> 27) & 0x1) == 0x1 ? 0x85 : 0x0)      /* data[59] */
          ^ (((data_a2_is_zero >> 26) & 0x1) == 0x1 ? 0x13 : 0x0)      /* data[58] */
          ^ (((data_a2_is_zero >> 25) & 0x1) == 0x1 ? 0x45 : 0x0)      /* data[57] */
          ^ (((data_a2_is_zero >> 24) & 0x1) == 0x1 ? 0x52 : 0x0)      /* data[56] */
          ^ (((data_a2_is_zero >> 23) & 0x1) == 0x1 ? 0x2a : 0x0)      /* data[55] */
          ^ (((data_a2_is_zero >> 22) & 0x1) == 0x1 ? 0x8a : 0x0)      /* data[54] */
          ^ (((data_a2_is_zero >> 21) & 0x1) == 0x1 ? 0x0b : 0x0)      /* data[53] */
          ^ (((data_a2_is_zero >> 20) & 0x1) == 0x1 ? 0x0e : 0x0)      /* data[52] */
          ^ (((data_a2_is_zero >> 19) & 0x1) == 0x1 ? 0xf8 : 0x0)      /* data[51] */
          ^ (((data_a2_is_zero >> 18) & 0x1) == 0x1 ? 0x25 : 0x0)      /* data[50] */
          ^ (((data_a2_is_zero >> 17) & 0x1) == 0x1 ? 0xd9 : 0x0)      /* data[49] */
          ^ (((data_a2_is_zero >> 16) & 0x1) == 0x1 ? 0xa1 : 0x0)      /* data[48] */
          ^ (((data_a2_is_zero >> 15) & 0x1) == 0x1 ? 0x54 : 0x0)      /* data[47] */
          ^ (((data_a2_is_zero >> 14) & 0x1) == 0x1 ? 0xa7 : 0x0)      /* data[46] */
          ^ (((data_a2_is_zero >> 13) & 0x1) == 0x1 ? 0xa8 : 0x0)      /* data[45] */
          ^ (((data_a2_is_zero >> 12) & 0x1) == 0x1 ? 0x92 : 0x0)      /* data[44] */
          ^ (((data_a2_is_zero >> 11) & 0x1) == 0x1 ? 0xc8 : 0x0)      /* data[43] */
          ^ (((data_a2_is_zero >> 10) & 0x1) == 0x1 ? 0x07 : 0x0)      /* data[42] */
          ^ (((data_a2_is_zero >>  9) & 0x1) == 0x1 ? 0x34 : 0x0)      /* data[41] */
          ^ (((data_a2_is_zero >>  8) & 0x1) == 0x1 ? 0x32 : 0x0)      /* data[40] */
          ^ (((data_a2_is_zero >>  7) & 0x1) == 0x1 ? 0x68 : 0x0)      /* data[39] */
          ^ (((data_a2_is_zero >>  6) & 0x1) == 0x1 ? 0x89 : 0x0)      /* data[38] */
          ^ (((data_a2_is_zero >>  5) & 0x1) == 0x1 ? 0x98 : 0x0)      /* data[37] */
          ^ (((data_a2_is_zero >>  4) & 0x1) == 0x1 ? 0x49 : 0x0)      /* data[36] */
          ^ (((data_a2_is_zero >>  3) & 0x1) == 0x1 ? 0x61 : 0x0)      /* data[35] */
          ^ (((data_a2_is_zero >>  2) & 0x1) == 0x1 ? 0x86 : 0x0)      /* data[34] */
          ^ (((data_a2_is_zero >>  1) & 0x1) == 0x1 ? 0x91 : 0x0)      /* data[33] */
          ^  ((data_a2_is_zero        & 0x1) == 0x1 ? 0x46 : 0x0);     /* data[32] */

    return ecc;
}

int32_t MPC5746RECC::encodeEcc32_lower (int32_t data_a2_is_one)
{
    int32_t ecc
        =   (((data_a2_is_one  >> 31) & 0x1) == 0x1 ? 0x58 : 0x0)      /* data[31] */
          ^ (((data_a2_is_one  >> 30) & 0x1) == 0x1 ? 0x4f : 0x0)      /* data[30] */
          ^ (((data_a2_is_one  >> 29) & 0x1) == 0x1 ? 0x38 : 0x0)      /* data[29] */
          ^ (((data_a2_is_one  >> 28) & 0x1) == 0x1 ? 0x75 : 0x0)      /* data[28] */
          ^ (((data_a2_is_one  >> 27) & 0x1) == 0x1 ? 0xc4 : 0x0)      /* data[27] */
          ^ (((data_a2_is_one  >> 26) & 0x1) == 0x1 ? 0x0d : 0x0)      /* data[26] */
          ^ (((data_a2_is_one  >> 25) & 0x1) == 0x1 ? 0xa4 : 0x0)      /* data[25] */
          ^ (((data_a2_is_one  >> 24) & 0x1) == 0x1 ? 0x37 : 0x0)      /* data[24] */
          ^ (((data_a2_is_one  >> 23) & 0x1) == 0x1 ? 0x64 : 0x0)      /* data[23] */
          ^ (((data_a2_is_one  >> 22) & 0x1) == 0x1 ? 0x16 : 0x0)      /* data[22] */
          ^ (((data_a2_is_one  >> 21) & 0x1) == 0x1 ? 0x94 : 0x0)      /* data[21] */
          ^ (((data_a2_is_one  >> 20) & 0x1) == 0x1 ? 0x29 : 0x0)      /* data[20] */
          ^ (((data_a2_is_one  >> 19) & 0x1) == 0x1 ? 0xea : 0x0)      /* data[19] */
          ^ (((data_a2_is_one  >> 18) & 0x1) == 0x1 ? 0x26 : 0x0)      /* data[18] */
          ^ (((data_a2_is_one  >> 17) & 0x1) == 0x1 ? 0x1a : 0x0)      /* data[17] */
          ^ (((data_a2_is_one  >> 16) & 0x1) == 0x1 ? 0x19 : 0x0)      /* data[16] */
          ^ (((data_a2_is_one  >> 15) & 0x1) == 0x1 ? 0xd0 : 0x0)      /* data[15] */
          ^ (((data_a2_is_one  >> 14) & 0x1) == 0x1 ? 0xc2 : 0x0)      /* data[14] */
          ^ (((data_a2_is_one  >> 13) & 0x1) == 0x1 ? 0x2c : 0x0)      /* data[13] */
          ^ (((data_a2_is_one  >> 12) & 0x1) == 0x1 ? 0x51 : 0x0)      /* data[12] */
          ^ (((data_a2_is_one  >> 11) & 0x1) == 0x1 ? 0xe0 : 0x0)      /* data[11] */
          ^ (((data_a2_is_one  >> 10) & 0x1) == 0x1 ? 0xa2 : 0x0)      /* data[10] */
          ^ (((data_a2_is_one  >>  9) & 0x1) == 0x1 ? 0x1c : 0x0)      /* data[ 9] */
          ^ (((data_a2_is_one  >>  8) & 0x1) == 0x1 ? 0x31 : 0x0)      /* data[ 8] */
          ^ (((data_a2_is_one  >>  7) & 0x1) == 0x1 ? 0x8c : 0x0)      /* data[ 7] */
          ^ (((data_a2_is_one  >>  6) & 0x1) == 0x1 ? 0x4a : 0x0)      /* data[ 6] */
          ^ (((data_a2_is_one  >>  5) & 0x1) == 0x1 ? 0x4c : 0x0)      /* data[ 5] */
          ^ (((data_a2_is_one  >>  4) & 0x1) == 0x1 ? 0x15 : 0x0)      /* data[ 4] */
          ^ (((data_a2_is_one  >>  3) & 0x1) == 0x1 ? 0x83 : 0x0)      /* data[ 3] */
          ^ (((data_a2_is_one  >>  2) & 0x1) == 0x1 ? 0x9e : 0x0)      /* data[ 2] */
          ^ (((data_a2_is_one  >>  1) & 0x1) == 0x1 ? 0x43 : 0x0)      /* data[ 1] */
          ^  ((data_a2_is_one         & 0x1) == 0x1 ? 0xc1 : 0x0);     /* data[ 0] */
    return(ecc);
}

int32_t MPC5746RECC::encodeEcc64(int32_t addr, int64_t flash_array) {
    int32_t lower = (int32_t) flash_array;
    int32_t upper = (int32_t) (flash_array << 32);

    int32_t ecc_addr = encodeEcc_address(addr);
    int32_t ecc_lower = encodeEcc32_lower(lower);
    int32_t ecc_upper = encodeEcc32_upper(upper);

    return ecc_addr ^ ecc_lower ^ ecc_upper;
}

int32_t MPC5746RECC::encodeEcc64(QByteArray addr, QByteArray flash_array)
{
    return encodeEcc64(
        QByteArray_toInt<uint32_t>(addr),
        QByteArray_toInt<uint64_t>(flash_array));
}

template <typename T>
T MPC5746RECC::QByteArray_toInt(QByteArray array)
{
    int sizeof_T = sizeof(T);
    int len = array.length();
    //array could have more bytes then type T has,
    //trim off all excess
    if (len > sizeof_T)
    {
        array.remove(sizeof_T, array.length() - sizeof_T);
        qDebug() << Q_FUNC_INFO << "Array contains" << len
                 << "bytes which is more then expected,"
                 << "trimming to first" << sizeof_T << "bytes.";
    }
    else if (array.length() < sizeof_T)
    {
        qDebug() << Q_FUNC_INFO << "Array contains" << len
                 << "bytes which is less then expected,"
                 << "assuming leading zeroes.";
    }
    //array length may have changed due to trimming
    len = array.length();
    int64_t res = (uint8_t)array.at(0);
    for (int i = 1; i < len; i++)
    {
        res = res << 8;
        res = res | (uint8_t)array.at(i);
    }
    return res;
}
