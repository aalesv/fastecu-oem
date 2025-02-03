#include "flash_ecu_subaru_denso_sh705x_kline.h"

FlashEcuSubaruDensoSH705xKline::FlashEcuSubaruDensoSH705xKline(SerialPortActions *serial, FileActions::EcuCalDefStructure *ecuCalDef, QString cmd_type, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EcuOperationsWindow)
    , ecuCalDef(ecuCalDef)
    , cmd_type(cmd_type)
{
    ui->setupUi(this);

    if (cmd_type == "test_write")
        this->setWindowTitle("Test write ROM " + ecuCalDef->FileName + " to ECU");
    else if (cmd_type == "write")
        this->setWindowTitle("Write ROM " + ecuCalDef->FileName + " to ECU");
    else if (cmd_type == "read")
        this->setWindowTitle("Read ROM from ECU");

    this->serial = serial;
}

void FlashEcuSubaruDensoSH705xKline::run()
{
    this->show();

    int result = STATUS_ERROR;
    set_progressbar_value(0);

    //result = init_flash_denso_kline_04(ecuCalDef, cmd_type);

    bool ok = false;

    mcu_type_string = ecuCalDef->McuType;
    mcu_type_index = 0;

    while (flashdevices[mcu_type_index].name != 0)
    {
        if (flashdevices[mcu_type_index].name == mcu_type_string)
            break;
        mcu_type_index++;
    }
    QString mcu_name = flashdevices[mcu_type_index].name;
    emit LOG_D("MCU type: " + mcu_name + " and index: " + mcu_type_index, true, true);

    kernel = ecuCalDef->Kernel;
    flash_method = ecuCalDef->FlashMethod;

    emit external_logger("Starting");

    if (cmd_type == "read")
    {
        emit LOG_I("Read memory with flashmethod '" + flash_method + "' and kernel '" + ecuCalDef->Kernel + "'", true, true);
    }
    else if (cmd_type == "test_write")
    {
        test_write = true;
        emit LOG_I("Test write memory with flashmethod '" + flash_method + "' and kernel '" + ecuCalDef->Kernel + "'", true, true);
    }
    else if (cmd_type == "write")
    {
        test_write = false;
        emit LOG_I("Write memory with flashmethod '" + flash_method + "' and kernel '" + ecuCalDef->Kernel + "'", true, true);
    }

    // Set serial port
    serial->set_is_iso14230_connection(false);
    serial->set_is_can_connection(false);
    serial->set_is_iso15765_connection(false);
    serial->set_is_29_bit_id(false);
    serial->set_serial_port_baudrate("4800");
    tester_id = 0xF0;
    target_id = 0x10;
    // Open serial port
    serial->open_serial_port();

    int ret = QMessageBox::warning(this, tr("Connecting to ECU"),
                                   tr("Turn ignition ON and press OK to start initializing connection to ECU"),
                                   QMessageBox::Ok | QMessageBox::Cancel,
                                   QMessageBox::Ok);

    switch (ret)
    {
        case QMessageBox::Ok:
            emit LOG_I("Connecting to Subaru 04 32-bit K-Line bootloader, please wait...", true, true);
            result = connect_bootloader_subaru_denso_sh705x_kline();

            if (result == STATUS_SUCCESS && !kernel_alive)
            {
                emit external_logger("Preparing, please wait...");
                emit LOG_I("Initializing Subaru 04 32-bit K-Line kernel upload, please wait...", true, true);
                result = upload_kernel_subaru_denso_sh705x_kline(kernel, ecuCalDef->KernelStartAddr.toUInt(&ok, 16));
            }
            if (result == STATUS_SUCCESS)
            {
                if (cmd_type == "read")
                {
                    emit external_logger("Reading ROM, please wait...");
                    emit LOG_I("Reading ROM from Subaru 04 32-bit using K-Line", true, true);
                    result = read_mem_subaru_denso_sh705x_kline(flashdevices[mcu_type_index].fblocks[0].start, flashdevices[mcu_type_index].romsize);
                }
                else if (cmd_type == "test_write" || cmd_type == "write")
                {
                    emit external_logger("Writing ROM, please wait...");
                    emit LOG_I("Writing ROM to Subaru 04 32-bit using K-Line", true, true);
                    result = write_mem_subaru_denso_sh705x_kline(test_write);
                }
            }
            emit external_logger("Finished");

            if (result == STATUS_SUCCESS)
            {
                QMessageBox::information(this, tr("ECU Operation"), "ECU operation was succesful, press OK to exit");
                this->close();
            }
            else
            {
                QMessageBox::warning(this, tr("ECU Operation"), "ECU operation failed, press OK to exit and try again");
            }
            break;
        case QMessageBox::Cancel:
            qDebug() << "Operation canceled";
            this->close();
            break;
        default:
            QMessageBox::warning(this, tr("Connecting to ECU"), "Unknown operation selected!");
            qDebug() << "Unknown operation selected!";
            this->close();
            break;
    }
}

FlashEcuSubaruDensoSH705xKline::~FlashEcuSubaruDensoSH705xKline()
{
    delete ui;
}

void FlashEcuSubaruDensoSH705xKline::closeEvent(QCloseEvent *bar)
{
    kill_process = true;
}

/*
 * Connect to Subaru Denso K-Line bootloader 32bit ECUs
 *
 * @return success
 */
int FlashEcuSubaruDensoSH705xKline::connect_bootloader_subaru_denso_sh705x_kline()
{
    QByteArray output;
    QByteArray received;
    QByteArray seed;
    QByteArray seed_key;

    QString msg;

    if (!serial->is_serial_port_open())
    {
        emit LOG_E("ERROR: Serial port is not open.", true, true);
        return STATUS_ERROR;
    }

    serial->change_port_speed("62500");
    serial->set_add_iso14230_header(true);
    serial->set_iso14230_startbyte(0x80);
    serial->set_iso14230_tester_id(0xFC);
    serial->set_iso14230_target_id(0x10);

    delay(100);

    emit LOG_I("Checking if kernel is already running...", true, true);

    received = request_kernel_init();
    if (received.length())
    {
        if ((uint8_t)received.at(1) == SID_KERNEL_INIT + 0x40)
        {
            emit LOG_I("Kernel already running, requesting kernel ID", true, true);
            delay(250);

            received = request_kernel_id();
            if (received == "")
                return STATUS_ERROR;

            received.remove(0, 2);
            emit LOG_I("Request kernel id ok: " + received, true, true);
            kernel_alive = true;
            return STATUS_SUCCESS;
        }
    }
    emit LOG_I("Kernel NOT running, requesting access", true, true);

    serial->set_iso14230_tester_id(0xF0);
    serial->change_port_speed("4800");
    serial->set_add_iso14230_header(false);
    delay(100);

    emit LOG_I("Initializing bootloader", true, true);

    // SSM init
    received = send_subaru_sid_bf_ssm_init();
    if (received == "")
        return STATUS_ERROR;
    received.remove(0, 8);
    received.remove(5, received.length() - 5);
    for (int i = 0; i < received.length(); i++)
    {
        msg.append(QString("%1").arg((uint8_t)received.at(i),2,16,QLatin1Char('0')).toUpper());
    }
    QString ecuid = msg;
    emit LOG_I("ECU ID = " + ecuid, true, true);

    // Start communication
    received = send_subaru_denso_sid_81_start_communication();
    //emit LOG_I("SID_81 = " + parse_message_to_hex(received), true, true);
    if (received == "" || (uint8_t)received.at(4) != 0xC1)
        return STATUS_ERROR;

    emit LOG_I("Start communication ok", true, true);

    // Request timing parameters
    received = send_subaru_denso_sid_83_request_timings();
    //emit LOG_I("SID_83 = " + parse_message_to_hex(received), true, true);
    if (received == "" || (uint8_t)received.at(4) != 0xC3)
        return STATUS_ERROR;

    emit LOG_I("Request timing parameters ok", true, true);

    // Request seed
    received = send_subaru_denso_sid_27_request_seed();
    //emit LOG_I("SID_27_01 = " + parse_message_to_hex(received), true, true);
    if (received == "" || (uint8_t)received.at(4) != 0x67)
        return STATUS_ERROR;

    emit LOG_I("Seed request ok", true, true);

    seed.append(received.at(6));
    seed.append(received.at(7));
    seed.append(received.at(8));
    seed.append(received.at(9));

    if (flash_method.endsWith("_ecutek"))
        seed_key = subaru_denso_generate_ecutek_kline_seed_key(seed);
    else
        seed_key = subaru_denso_generate_kline_seed_key(seed);
    msg = parse_message_to_hex(seed_key);
    //emit LOG_I("Calculated seed key: " + msg + ", sending to ECU", true, true);
    emit LOG_I("Sending seed key", true, true);

    received = send_subaru_denso_sid_27_send_seed_key(seed_key);
    //emit LOG_I("SID_27_02 = " + parse_message_to_hex(received), true, true);
    if (received == "" || (uint8_t)received.at(4) != 0x67)
        return STATUS_ERROR;

    emit LOG_I("Seed key ok", true, true);

    // Start diagnostic session
    received = send_subaru_denso_sid_10_start_diagnostic();
    if (received == "" || (uint8_t)received.at(4) != 0x50)
        return STATUS_ERROR;

    emit LOG_I("Start diagnostic session ok", true, true);

    return STATUS_SUCCESS;
}

/*
 * Upload kernel to Subaru Denso K-Line 32bit ECUs
 *
 * @return success
 */
int FlashEcuSubaruDensoSH705xKline::upload_kernel_subaru_denso_sh705x_kline(QString kernel, uint32_t kernel_start_addr)
{
    QFile file(kernel);

    QByteArray output;
    QByteArray payload;
    QByteArray received;
    QByteArray msg;
    QByteArray pl_encr;
    uint32_t file_len = 0;
    uint32_t pl_len = 0;
    uint32_t start_address = 0;
    uint32_t len = 0;
    QByteArray cks_bypass;
    uint8_t chk_sum = 0;

    QString mcu_name;

    start_address = kernel_start_addr;//flashdevices[mcu_type_index].kblocks->start;
    emit LOG_D("Start address to upload kernel: " + QString::number(start_address), true, true);

    if (!serial->is_serial_port_open())
    {
        emit LOG_E("ERROR: Serial port is not open.", true, true);
        return STATUS_ERROR;
    }

    serial->set_add_iso14230_header(false);

    // Check kernel file
    if (!file.open(QIODevice::ReadOnly ))
    {
        emit LOG_E("Unable to open kernel file for reading", true, true);
        return STATUS_ERROR;
    }
    file_len = file.size();
    pl_len = (file_len + 3) & ~3;
    pl_encr = file.readAll();
    len = pl_len &= ~3;

    if (file_len != pl_len)
    {
        //emit LOG_I("Using " + QString::number(file_len) + " byte payload, padding with garbage to " + QString::number(pl_len) + " (" + QString::number(file_len) + ") bytes.\n", true, true);
    }
    else
    {
        //emit LOG_I("Using " + QString::number(file_len) + " (" + QString::number(file_len) + ") byte payload.", true, true);
    }
    if ((uint32_t)pl_encr.size() != file_len)
    {
        emit LOG_E("File size error (" + QString::number(file_len) + "/" + QString::number(pl_encr.size()) + ")", true, true);
        return STATUS_ERROR;
    }

    // Change port speed to upload kernel
    if (serial->change_port_speed("15625"))
        return STATUS_ERROR;

    // Request upload
    emit LOG_I("Send 'sid34_request_upload'", true, true);
    received = send_subaru_denso_sid_34_request_upload(start_address, len);
    if (received == "" || (uint8_t)received.at(4) != 0x74)
        return STATUS_ERROR;

    emit LOG_I("Kernel upload request ok, uploading now, please wait...", true, true);
    qDebug() << "Kernel upload request ok, uploading now, please wait...";

    //qDebug() << "Encrypt kernel data before upload";
    //pl_encr = sub_encrypt_buf(pl_encr, (uint32_t) pl_len);
    pl_encr = subaru_denso_encrypt_32bit_payload(pl_encr, pl_len);

    emit LOG_I("Send 'sid36_transfer_data'", true, true);
    //qDebug() << "Send 'sid36_transfer_data'";
    received = send_subaru_denso_sid_36_transferdata(start_address, pl_encr, len);
    if (received == "" || (uint8_t)received.at(4) != 0x76)
        return STATUS_ERROR;

    emit LOG_I("Kernel uploaded", true, true);

    /* sid34 request upload - checksum bypass put just after payload */
    emit LOG_I("Send 'sid34_transfer_data'", true, true);
    received = send_subaru_denso_sid_34_request_upload(start_address + len, 4);
    if (received == "" || (uint8_t)received.at(4) != 0x74)
        return STATUS_ERROR;

    cks_bypass.append((uint8_t)0x00);
    cks_bypass.append((uint8_t)0x00);
    cks_bypass.append((uint8_t)0x5A);
    cks_bypass.append((uint8_t)0xA5);

    cks_bypass = subaru_denso_encrypt_32bit_payload(cks_bypass, (uint32_t) 4);

    /* sid36 transferData for checksum bypass */
    emit LOG_D("Send 'sid36_transfer_data' for chksum bypass", true, true);
    received = send_subaru_denso_sid_36_transferdata(start_address + len, cks_bypass, 4);
    if (received == "" || (uint8_t)received.at(4) != 0x76)
        return STATUS_ERROR;

    emit LOG_I("Checksum bypass ok", true, true);

    /* SID 37 TransferExit does not exist on all Subaru ROMs */

    /* RAMjump ! */
    emit LOG_D("Send 'sid31_transfer_data' to jump to kernel", true, true);
    received = send_subaru_denso_sid_31_start_routine();
    if (received == "" || (uint8_t)received.at(4) != 0x71)
        return STATUS_ERROR;

    emit LOG_I("Kernel started, initializing...", true, true);

    serial->change_port_speed("62500");
    serial->set_add_iso14230_header(true);
    serial->set_iso14230_startbyte(0x80);
    serial->set_iso14230_tester_id(0xFC);
    serial->set_iso14230_target_id(0x10);

    delay(100);

    received = request_kernel_init();
    if (received == "")
    {
        emit LOG_E("Kernel init NOK! No response from kernel" + parse_message_to_hex(received), true, true);
        return STATUS_ERROR;
    }
    if ((uint8_t)received.at(1) != SID_KERNEL_INIT + 0x40)
    {
        emit LOG_E("Kernel init NOK! Got bad startcomm response from kernel" + parse_message_to_hex(received), true, true);
        return STATUS_ERROR;
    }
    else
    {
        emit LOG_I("Kernel init OK", true, true);
    }

    delay(100);

    emit LOG_I("Requesting kernel ID", true, true);

    received = request_kernel_id();
    if (received == "")
        return STATUS_ERROR;

    emit LOG_I("Kernel ID: " + received, true, true);

    return STATUS_SUCCESS;
}

/*
 * Read memory from Subaru Denso K-Line 32bit ECUs, nisprog kernel
 *
 * @return success
 */
int FlashEcuSubaruDensoSH705xKline::read_mem_subaru_denso_sh705x_kline(uint32_t start_addr, uint32_t length)
{
    QElapsedTimer timer;
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    QByteArray mapdata;
    uint32_t cplen = 0;

    uint32_t skip_start = start_addr & (32 - 1); //if unaligned, we'll be receiving this many extra bytes
    uint32_t addr = start_addr - skip_start;
    uint32_t willget = (skip_start + length + 31) & ~(32 - 1);
    uint32_t len_done = 0;  //total data written to file

    #define NP10_MAXBLKS    32   //# of blocks to request per loop. Too high might flood us
    serial->set_add_iso14230_header(true);

    output.append(SID_DUMP);
    output.append(SID_DUMP_ROM);
    output.append((uint8_t)0x00);
    output.append((uint8_t)0x00);
    output.append((uint8_t)0x00);
    output.append((uint8_t)0x00);

    timer.start();
    set_progressbar_value(0);

    mapdata.clear();
    while (willget)
    {
        if (kill_process)
            return STATUS_ERROR;

        uint32_t numblocks = 0;
        unsigned curspeed = 0, tleft;
        float pleft = 0;
        unsigned long chrono;

        //delay(1);
        numblocks = willget / 32;

        if (numblocks > NP10_MAXBLKS)
            numblocks = NP10_MAXBLKS;

        uint32_t curblock = (addr / 32);

        uint32_t pagesize = numblocks * 32;
        pleft = (float)(addr - start_addr) / (float)length * 100.0f;
        set_progressbar_value(pleft);


        output[2] = numblocks >> 8;
        output[3] = numblocks >> 0;

        output[4] = curblock >> 8;
        output[5] = curblock >> 0;

        received = serial->write_serial_data_echo_check(output);
        //delay(10);
        // Receive map data, check and remove header // ADJUST THIS LATER //
        //received.clear();
        received = serial->read_serial_data(numblocks * (32 + 3), serial_read_short_timeout);
        //emit LOG_D("Response: " + parse_message_to_hex(received), true, true);
        if (!received.length())
        {
            delay(500);
            received = serial->write_serial_data_echo_check(output);
            received = serial->read_serial_data(numblocks * (32 + 3), serial_read_short_timeout);
            //emit LOG_D("Response: " + parse_message_to_hex(received), true, true);
            if (!received.length())
            {
                emit LOG_E("Response: " + parse_message_to_hex(received), true, true);
                return STATUS_ERROR;
            }
        }
        for (uint32_t i = 0; i < numblocks; i++)
        {
            received.remove(0, 2);
            mapdata.append(received, 32);
            received.remove(0, 33);
        }
        // don't count skipped first bytes //
        cplen = (numblocks * 32) - skip_start; //this is the actual # of valid bytes in buf[]
        skip_start = 0;

        chrono = timer.elapsed();
        timer.start();

        if (cplen > 0 && chrono > 0)
            curspeed = cplen * (1000.0f / chrono);

        if (!curspeed) {
            curspeed += 1;
        }

        tleft = (willget / curspeed) % 9999;
        tleft++;

        QString start_address = QString("%1").arg(addr,8,16,QLatin1Char('0')).toUpper();
        QString block_len = QString("%1").arg(pagesize,8,16,QLatin1Char('0')).toUpper();
        msg = QString("Kernel read addr:  0x%1  length:  0x%2,  %3  B/s  %4 s remaining").arg(start_address).arg(block_len).arg(curspeed, 6, 10, QLatin1Char(' ')).arg(tleft, 6, 10, QLatin1Char(' ')).toUtf8();
        emit LOG_I(msg, true, true);
        delay(1);

        // and drop extra bytes at the end //
        uint32_t extrabytes = (cplen + len_done);   //hypothetical new length
        if (extrabytes > length) {
            cplen -= (extrabytes - length);
            //thus, (len_done + cplen) will not exceed len
        }

        // increment addr, len, etc //
        len_done += cplen;
        addr += (numblocks * 32);
        willget -= (numblocks * 32);

    }

    ecuCalDef->FullRomData = mapdata;
    set_progressbar_value(100);

    return STATUS_SUCCESS;
}

/*
 * Write memory to Subaru Denso K-Line 32bit ECUs, nisprog kernel
 *
 * @return success
 */
int FlashEcuSubaruDensoSH705xKline::write_mem_subaru_denso_sh705x_kline(bool test_write)
{
    QByteArray filedata;

    filedata = ecuCalDef->FullRomData;

    QScopedArrayPointer<uint8_t> data_array(new uint8_t[filedata.length()]);

    int block_modified[16] = {0};

    unsigned bcnt = 0;
    unsigned blockno;

    set_progressbar_value(0);

    for (int i = 0; i < filedata.length(); i++)
    {
        data_array[i] = filedata.at(i);
    }

    emit LOG_I("--- Comparing ECU flash memory pages to image file ---", true, true);
    emit LOG_I("blk\tstart\tlen\tecu crc\timg crc\tsame?", true, true);

    if (get_changed_blocks_denso_sh705x_kline(&data_array[0], block_modified))
    {
        emit LOG_E("Error in ROM compare", true, true);
        return STATUS_ERROR;
    }

    bcnt = 0;
    emit LOG_I("Different blocks: ", true, false);
    for (blockno = 0; blockno < flashdevices[mcu_type_index].numblocks; blockno++) {
        if (block_modified[blockno]) {
            emit LOG_I(QString::number(blockno) + ", ", false, false);
            bcnt += 1;
        }
    }
    emit LOG_I(" (total: " + QString::number(bcnt) + ")", false, true);

    if (bcnt)
    {
        flashbytescount = 0;
        flashbytesindex = 0;

        for (blockno = 0; blockno < flashdevices[mcu_type_index].numblocks; blockno++)
        {
            if (block_modified[blockno])
            {
                flashbytescount += flashdevices[mcu_type_index].fblocks[blockno].len;
            }
        }

        emit LOG_I("--- Start writing ROM file to ECU flash memory ---", true, true);
        for (blockno = 0; blockno < flashdevices[mcu_type_index].numblocks; blockno++)
        {
            if (block_modified[blockno])
            {
                if (reflash_block_denso_sh705x_kline(&data_array[flashdevices[mcu_type_index].fblocks->start], &flashdevices[mcu_type_index], blockno, test_write))
                {
                    emit LOG_I("Block " + QString::number(blockno) + " reflash failed.", true, true);
                    return STATUS_ERROR;
                }
                else
                {
                    flashbytesindex += flashdevices[mcu_type_index].fblocks[blockno].len;
                    emit LOG_I("Block " + QString::number(blockno) + " reflash complete.", true, true);
                }
            }
        }
        set_progressbar_value(100);

        emit LOG_I("--- Comparing ECU flash memory pages to image file after reflash ---", true, true);
        emit LOG_I("blk\tstart\tlen\tecu crc\timg crc\tsame?", true, true);

        if (get_changed_blocks_denso_sh705x_kline(&data_array[0], block_modified))
        {
            emit LOG_E("Error in ROM compare", true, true);
            return STATUS_ERROR;
        }

        bcnt = 0;
        emit LOG_I("Different blocks: ", true, false);
        for (blockno = 0; blockno < flashdevices[mcu_type_index].numblocks; blockno++) {
            if (block_modified[blockno])
            {
                emit LOG_I(QString::number(blockno) + ", ", false, false);
                bcnt += 1;
            }
        }
        emit LOG_I(" (total: " + QString::number(bcnt) + ")", false, true);
        if (!test_write)
        {
            if (bcnt)
            {
                emit LOG_E("*** ERROR IN FLASH PROCESS ***", true, true);
                emit LOG_E("Don't power off your ECU, kernel is still running and you can try flashing again!", true, true);
            }
        }
        else
            emit LOG_I("*** Test write PASS, it's ok to perform actual write! ***", true, true);
    }
    else
    {
        emit LOG_I("*** Compare results no difference between ROM and ECU data, no flashing needed! ***", true, true);
    }

    return STATUS_SUCCESS;
}

/*
 * Compare ROM 32bit K-Line ECUs, nisprog kernel
 *
 * @return
 */
int FlashEcuSubaruDensoSH705xKline::get_changed_blocks_denso_sh705x_kline(const uint8_t *src, int *modified)
{
    unsigned blockno;
    QByteArray msg;
    for (blockno = 0; blockno < flashdevices[mcu_type_index].numblocks; blockno++) {
        if (kill_process)
            return STATUS_ERROR;

        uint32_t bs, blen;
        bs = flashdevices[mcu_type_index].fblocks[blockno].start;
        blen = flashdevices[mcu_type_index].fblocks[blockno].len;

        QString block_no = QString("%1").arg((uint8_t)blockno,2,10,QLatin1Char('0')).toUpper();
        QString block_start = QString("%1").arg((uint32_t)bs,8,16,QLatin1Char('0')).toUpper();
        QString block_length = QString("%1").arg((uint32_t)blen,8,16,QLatin1Char('0')).toUpper();

        msg = QString("FB" + block_no + "\t" + block_start + "\t" + block_length).toUtf8();
        emit LOG_I(msg, true, false);
        // do CRC comparison with ECU //
        if (check_romcrc_denso_sh705x_kline(&src[bs], bs, blen, &modified[blockno])) {
            return STATUS_ERROR;
        }
    }
    return 0;
}

/*
 * ROM CRC 32bit K-Line ECUs, nisprog kernel
 *
 * @return
 */
int FlashEcuSubaruDensoSH705xKline::check_romcrc_denso_sh705x_kline(const uint8_t *src, uint32_t start, uint32_t len, int *modified)
{
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    uint32_t imgcrc32 = 0;
    uint32_t ecucrc32 = 0;
    uint32_t pagesize = len; // Test 32-bit CRC with block size

    // Test 32-bit CRC with block size
    output.clear();
    output.append(SID_CONF);
    output.append(SID_CONF_CKS1);
    output.append((uint8_t)(start >> 16) & 0xFF);
    output.append((uint8_t)(start >> 8) & 0xFF);
    output.append((uint8_t)start & 0xFF);
    output.append((uint8_t)(pagesize >> 16) & 0xFF);
    output.append((uint8_t)(pagesize >> 8) & 0xFF);
    output.append((uint8_t)pagesize & 0xFF);
    qDebug() << "Send: " + parse_message_to_hex(output);
    delay(100);
    serial->write_serial_data_echo_check(output);
    delay(200);
    received.clear();
    received = serial->read_serial_data(10, serial_read_long_timeout);
    emit LOG_D("Response: " + parse_message_to_hex(received), true, true);
    if (received.length())
    {
        if (received.at(1) == 0x7f)
        {
            emit LOG_E("", false, true);
            emit LOG_E("Failed: Wrong answer from ECU", true, true);
            return STATUS_ERROR;
        }
        uint8_t len = (uint8_t)received.at(0);
        if (len > 5)
        {
            received.remove(0, 3);
            received.remove(received.length() - 1, 1);
        }
    }
    else
    {
        emit LOG_E("", false, true);
        emit LOG_E("Failed: No answer from ECU", true, true);
        return STATUS_ERROR;
    }

    imgcrc32 = crc32(src, pagesize);
    if (received.length() > 3)
        ecucrc32 = ((uint8_t)received.at(0) << 24) | ((uint8_t)received.at(1) << 16) | ((uint8_t)received.at(2) << 8) | (uint8_t)received.at(3);
    msg.clear();
    //msg.append(QString("ROM CRC: 0x%1 IMG CRC: 0x%2").arg(ecucrc32,8,16,QLatin1Char('0')).arg(imgcrc32,8,16,QLatin1Char('0')).toUtf8());

    QString ecu_crc32 = QString("%1").arg((uint32_t)ecucrc32,8,16,QLatin1Char('0')).toUpper();
    QString img_crc32 = QString("%1").arg((uint32_t)imgcrc32,8,16,QLatin1Char('0')).toUpper();
    msg = QString("\t" + ecu_crc32 + "\t" + img_crc32).toUtf8();
    emit LOG_I(msg, false, false);
    if (ecucrc32 != imgcrc32)
    {
        emit LOG_I("\tNO", false, true);
        *modified = 1;
        serial->read_serial_data(100, serial_read_short_timeout);
        return 0;
    }

    emit LOG_I("\tYES", false, true);
    *modified = 0;
    serial->read_serial_data(100, serial_read_short_timeout);
    return 0;
}

unsigned int FlashEcuSubaruDensoSH705xKline::crc32(const unsigned char *buf, unsigned int len)
{
    unsigned int crc = 0xFFFFFFFF;

    if (!crc_tab32_init)
        init_crc32_tab();

    if (buf == NULL)
        return 0L;
    while (len--)
        crc = crc_tab32[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFF;
}

void FlashEcuSubaruDensoSH705xKline::init_crc32_tab( void ) {
    uint32_t i, j;
    uint32_t crc, c;

    for (i=0; i<256; i++) {
        crc = 0;
        c = (uint32_t)i;

        for (j=0; j<8; j++) {
            if ( (crc ^ c) & 0x00000001 )
                crc = ( crc >> 1 ) ^ CRC32;
            else
                crc =   crc >> 1;
            c = c >> 1;
        }
        crc_tab32[i] = crc;
    }

    crc_tab32_init = 1;

}  /* init_crc32_tab */

/*
 *  Reflash ROM 32bit K-Line ECUs
 *
 *  @return
 */
int FlashEcuSubaruDensoSH705xKline::reflash_block_denso_sh705x_kline(const uint8_t *newdata, const struct flashdev_t *fdt, unsigned blockno, bool test_write)
{
    int errval;

    uint32_t start;
    uint32_t len;

    QByteArray output;
    QByteArray received;
    QByteArray msg;

    if (blockno >= fdt->numblocks) {
        emit LOG_I("block " + QString::number(blockno) + " out of range !", true, true);
        return STATUS_ERROR;
    }

    start = fdt->fblocks[blockno].start;
    len = fdt->fblocks[blockno].len;

    QString start_addr = QString("%1").arg((uint32_t)start,8,16,QLatin1Char('0')).toUpper();
    QString length = QString("%1").arg((uint32_t)len,8,16,QLatin1Char('0')).toUpper();
    msg = QString("Flash block addr: 0x" + start_addr + " len: 0x" + length).toUtf8();
    emit LOG_I(msg, true, true);

    // 1- requestdownload //
    output.append(SID_FLREQ);
    //received.clear();
    received = serial->write_serial_data_echo_check(output);

    //received.clear();
    received = serial->read_serial_data(8, serial_read_short_timeout);
    if (received.length() < 2)
    {
        emit LOG_E("No response from ECU", true, true);
        return STATUS_ERROR;
    }
    if ((uint8_t)received.at(1) != (SID_FLREQ + 0x40))
    {
        emit LOG_E("Wrong response from ECU", true, true);//;
        emit LOG_E("Response: " + parse_message_to_hex(received), true, true);
        output.clear();
        output.append(SID_CONF_LASTERR);
        received = serial->write_serial_data_echo_check(output);
        received = serial->read_serial_data(8, serial_read_short_timeout);
        emit LOG_E("SID_FLREQ failed with errcode", true, true);//;
        emit LOG_E("SID_CONF_LASTERR: " + parse_message_to_hex(received), true, true);
        return STATUS_ERROR;
    }

    // 2- Unprotect maybe //
    output.clear();
    if (!test_write)
    {
        output.append(SID_FLASH);
        output.append(SIDFL_UNPROTECT);
        output.append(~SIDFL_UNPROTECT);
        received = serial->write_serial_data_echo_check(output);

        //received.clear();
        received = serial->read_serial_data(3, serial_read_medium_timeout);
        if (received.length() < 2)
        {
            emit LOG_E("no 'unprotect' response", true, true);
            return STATUS_ERROR;
        }
        if ((uint8_t)received.at(1) != (SID_FLASH + 0x40)) {
            emit LOG_E("got bad Unprotect response: " + parse_message_to_hex(received), true, true);
            return STATUS_ERROR;
        }
        emit LOG_E("SID_UNPROTECT: " + parse_message_to_hex(received), true, true);
    }
    else
        emit LOG_I("ECU write in test_write mode, no real write processed.", true, true);

    // 3- erase block //
    msg = QString("Erasing block %1 (0x%2-0x%3)...").arg((uint8_t)blockno,2,16,QLatin1Char('0')).arg((uint8_t)(unsigned) start,8,16,QLatin1Char('0')).arg((uint32_t)(unsigned) start + len - 1,8,16,QLatin1Char('0')).toUtf8();
    output.clear();
    output.append(SID_FLASH);
    output.append(SIDFL_EB);
    output.append(blockno);
    received = serial->write_serial_data_echo_check(output);
    //received = serial->read_serial_data(1, serial_read_short_timeout);

    emit LOG_I("Erase block: " + QString::number(blockno), true, true);

    received.clear();
    //delay(2000);
    //received = serial->read_serial_data(3, serial_read_short_timeout);

    QTime dieTime = QTime::currentTime().addMSecs(serial_read_extra_long_timeout);
    while ((uint32_t)received.length() < 3 && (QTime::currentTime() < dieTime))
    {
        received = serial->read_serial_data(3, serial_read_short_timeout);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        delay(100);
    }

    emit LOG_I("SID_FLASH | SIDFL_EB: " + parse_message_to_hex(received), true, true);
    if (received.length() < 3)
    {
        emit LOG_E("no 'ERASE_BLOCK' response", true, true);
        return STATUS_ERROR;
    }
    if ((uint8_t)received.at(1) != (SID_FLASH + 0x40))
    {
        emit LOG_E("got bad ERASE_BLOCK response : ", true, true);
        emit LOG_E("SIDFL_EB: " + parse_message_to_hex(received), true, true);
        return STATUS_ERROR;
    }

    // 4- write //
    errval = flash_block_denso_sh705x_kline(newdata, start, len);
    if (errval) {
        emit LOG_E("Reflash error! Do not panic, do not reset the ECU immediately. The kernel is most likely still running and receiving commands!", true, true);
        return STATUS_ERROR;
    }

    emit LOG_I("Flash block ok", true, true);

    return STATUS_SUCCESS;
}

/*
 * Flash block 32bit K-Line ECUs, nisprog kernel
 *
 * @return
 */
int FlashEcuSubaruDensoSH705xKline::flash_block_denso_sh705x_kline(const uint8_t *src, uint32_t start, uint32_t len)
{

    // program 128-byte chunks //
    uint32_t remain = len;
    uint32_t byteindex = flashbytesindex;
    uint8_t blocksize = 128;

    QElapsedTimer timer;
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    QByteArray chksum_data;

    unsigned long chrono;
    unsigned curspeed, tleft;

    if ((len & (blocksize - 1)) ||
        (start & (blocksize - 1))) {
        emit LOG_E("error: misaligned start / length!", true, true);
        return STATUS_ERROR;
    }

    timer.start();
    while (remain) {
        if (kill_process)
            return STATUS_ERROR;

        //delay(1);
        chksum_data.clear();
        output.clear();
        chksum_data.append(start >> 16);
        chksum_data.append(start >> 8);
        chksum_data.append(start >> 0);
        for (unsigned i = start; i < (start + blocksize); i++)
        {
            chksum_data.append(src[i]);
        }
        chksum_data.append(cks_add8(chksum_data, 131));
        output.append(SID_FLASH);
        output.append(SIDFL_WB);
        output.append(chksum_data);

        received = serial->write_serial_data_echo_check(output);

        received = serial->read_serial_data(3, serial_read_medium_timeout);
        if (received.length() <= 1) {
            emit LOG_E("npk_raw_flashblock: no response @ " + QString::number((unsigned) start), true, true);
            return STATUS_ERROR;
        }
        if (received.length() < 3) {
            emit LOG_E("npk_raw_flashblock: incomplete response @ " + QString::number((unsigned) start), true, true);
            return STATUS_ERROR;
        }

        if ((uint8_t)received.at(1) != (SID_FLASH + 0x40))
        {
            //maybe negative response, if so, get the remaining packet
            emit LOG_E("npk_raw_flashblock: bad response @ " + QString::number((unsigned) start), true, true);

            int needed = 1 + (uint8_t)received.at(0) - received.length();
            if (needed > 0) {
                received.append(serial->read_serial_data(needed, serial_read_medium_timeout));
            }
            emit LOG_E("npk_raw_flashblock: " + parse_message_to_hex(received), true, true);
            return STATUS_ERROR;
        }

        QString start_address = QString("%1").arg(start,8,16,QLatin1Char('0')).toUpper();
        msg = QString("Writing chunk @ 0x%1 (%2\% - %3 B/s, ~ %4 s remaining)").arg(start_address).arg((unsigned) 100 * (len - remain) / len,1,10,QLatin1Char('0')).arg((uint32_t)curspeed,1,10,QLatin1Char('0')).arg(tleft,1,10,QLatin1Char('0')).toUtf8();
        emit LOG_I(msg, true, true);

        remain -= blocksize;
        start += blocksize;
        byteindex += blocksize;
        //src += blocksize;

        chrono = timer.elapsed();
        timer.start();

        if (!chrono) {
            chrono += 1;
        }
        curspeed = blocksize * (1000.0f / chrono);  //avg B/s
        if (!curspeed) {
            curspeed += 1;
        }

        tleft = ((float)flashbytescount - byteindex) / curspeed;  //s
        if (tleft > 9999) {
            tleft = 9999;
        }
        tleft++;

        float pleft = (float)byteindex / (float)flashbytescount * 100.0f;
        set_progressbar_value(pleft);
    }

    emit LOG_I("npk_raw_flashblock: write complete.", true, true);
    received = serial->read_serial_data(100, serial_read_short_timeout);
    return 0;
}





/*
 * 8bit checksum
 *
 * @return
 */
uint8_t FlashEcuSubaruDensoSH705xKline::cks_add8(QByteArray chksum_data, unsigned len)
{
    uint16_t sum = 0;
    uint8_t data[chksum_data.length()];
/*
    for (int i = 0; i < chksum_data.length(); i++)
    {
        data[i] = chksum_data.at(i);
    }
*/
    for (unsigned i = 0; i < len; i++) {
        sum += (uint8_t)chksum_data.at(i);//data[i];
        if (sum & 0x100) {
            sum += 1;
        }
        sum = (uint8_t) sum;
    }
    return sum;
}












/*
 * ECU init
 *
 * @return ECU ID and capabilities
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_sid_bf_ssm_init()
{
    QByteArray output;
    QByteArray received;
    uint8_t loop_cnt = 0;


    output.append((uint8_t)0xBF);

    while (received == "" && loop_cnt < 5)
    {
        if (kill_process)
            break;

        emit LOG_I("SSM init", true, true);
        serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
        delay(500);
        received = serial->read_serial_data(100, receive_timeout);
        loop_cnt++;
    }

    return received;
}

/*
 * Start diagnostic connection
 *
 * @return received response
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_81_start_communication()
{
    QByteArray output;
    QByteArray received;
    uint8_t loop_cnt = 0;

    output.append((uint8_t)0x81);
    serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
    received = serial->read_serial_data(8, receive_timeout);

    return received;
}

/*
 * Start diagnostic connection
 *
 * @return received response
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_83_request_timings()
{
    QByteArray output;
    QByteArray received;
    uint8_t loop_cnt = 0;

    output.append((uint8_t)0x83);
    output.append((uint8_t)0x00);
    serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
    received = serial->read_serial_data(12, receive_timeout);

    return received;
}

/*
 * Request seed
 *
 * @return seed (4 bytes)
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_27_request_seed()
{
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    uint8_t loop_cnt = 0;

    output.append((uint8_t)0x27);
    output.append((uint8_t)0x01);
    received = serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
    received = serial->read_serial_data(11, receive_timeout);

    return received;
}

/*
 * Send seed key
 *
 * @return received response
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_27_send_seed_key(QByteArray seed_key)
{
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    uint8_t loop_cnt = 0;

    output.append((uint8_t)0x27);
    output.append((uint8_t)0x02);
    output.append(seed_key);
    serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
    received = serial->read_serial_data(8, receive_timeout);

    return received;
}

/*
 * Request start diagnostic
 *
 * @return received response
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_10_start_diagnostic()
{
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    uint8_t loop_cnt = 0;

    output.append((uint8_t)0x10);
    output.append((uint8_t)0x85);
    output.append((uint8_t)0x02);
    serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
    received = serial->read_serial_data(7, receive_timeout);
    /*
    while (received == "" && loop_cnt < comm_try_count)
    {
        serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
        delay(comm_try_timeout);
        received = serial->read_serial_data(7, receive_timeout);
        loop_cnt++;
    }
*/
    return received;
}

/*
 * Request data upload (kernel)
 *
 * @return received response
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_34_request_upload(uint32_t dataaddr, uint32_t datalen)
{
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    uint8_t loop_cnt = 0;

    output.append((uint8_t)0x34);
    output.append((uint8_t)(dataaddr >> 16) & 0xFF);
    output.append((uint8_t)(dataaddr >> 8) & 0xFF);
    output.append((uint8_t)dataaddr & 0xFF);
    output.append((uint8_t)0x04);
    output.append((uint8_t)(datalen >> 16) & 0xFF);
    output.append((uint8_t)(datalen >> 8) & 0xFF);
    output.append((uint8_t)datalen & 0xFF);

    serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
    received = serial->read_serial_data(7, receive_timeout);

    return received;
}

/*
 * Transfer data (kernel)
 *
 * @return received response
 */
QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_36_transferdata(uint32_t dataaddr, QByteArray buf, uint32_t len)
{
    QByteArray output;
    QByteArray received;
    uint32_t blockaddr = 0;
    uint16_t blockno = 0;
    uint16_t maxblocks = 0;
    uint8_t loop_cnt = 0;

    len &= ~0x03;
    if (!buf.length() || !len) {
        emit LOG_E("Error in kernel data length!", true, true);
        return NULL;
    }

    maxblocks = (len - 1) >> 7;  // number of 128 byte blocks - 1

    set_progressbar_value(0);

    for (blockno = 0; blockno <= maxblocks; blockno++)
    {
        if (kill_process)
            return NULL;

        blockaddr = dataaddr + blockno * 128;
        output.clear();
        output.append(0x36);
        output.append((uint8_t)(blockaddr >> 16) & 0xFF);
        output.append((uint8_t)(blockaddr >> 8) & 0xFF);
        output.append((uint8_t)blockaddr & 0xFF);

        if (blockno == maxblocks)
        {
            for (uint32_t i = 0; i < len; i++)
            {
                output.append(buf.at(i + blockno * 128));
            }
        }
        else
        {
            for (int i = 0; i < 128; i++)
            {
                output.append(buf.at(i + blockno * 128));
            }
            len -= 128;
        }

        serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
        received = serial->read_serial_data(6, receive_timeout);

        float pleft = (float)blockno / (float)maxblocks * 100;
        set_progressbar_value(pleft);
    }

    set_progressbar_value(100);

    return received;

}

QByteArray FlashEcuSubaruDensoSH705xKline::send_subaru_denso_sid_31_start_routine()
{
    QByteArray output;
    QByteArray received;
    uint8_t loop_cnt = 0;

    output.append((uint8_t)0x31);
    output.append((uint8_t)0x01);
    output.append((uint8_t)0x01);
    serial->write_serial_data_echo_check(add_ssm_header(output, tester_id, target_id, false));
    received = serial->read_serial_data(8, receive_timeout);

    return received;
}

/*
 * Generate denso kline seed key from received seed bytes
 *
 * @return seed key (4 bytes)
 */
QByteArray FlashEcuSubaruDensoSH705xKline::subaru_denso_generate_kline_seed_key(QByteArray requested_seed)
{
    QByteArray key;

    const uint16_t keytogenerateindex_1[]={
        0x53DA, 0x33BC, 0x72EB, 0x437D,
        0x7CA3, 0x3382, 0x834F, 0x3608,
        0xAFB8, 0x503D, 0xDBA3, 0x9D34,
        0x3563, 0x6B70, 0x6E74, 0x88F0
    };

    const uint16_t keytogenerateindex_2[]={
        0x24B9, 0x9D91, 0xFF0C, 0xB8D5,
        0x15BB, 0xF998, 0x8723, 0x9E05,
        0x7092, 0xD683, 0xBA03, 0x59E1,
        0x6136, 0x9B9A, 0x9CFB, 0x9DDB
    };

    const uint8_t indextransformation[]={
        0x5, 0x6, 0x7, 0x1, 0x9, 0xC, 0xD, 0x8,
        0xA, 0xD, 0x2, 0xB, 0xF, 0x4, 0x0, 0x3,
        0xB, 0x4, 0x6, 0x0, 0xF, 0x2, 0xD, 0x9,
        0x5, 0xC, 0x1, 0xA, 0x3, 0xD, 0xE, 0x8
    };

    key = subaru_denso_calculate_seed_key(requested_seed, keytogenerateindex_1, indextransformation);

    return key;
}

/*
 * Generate denso kline ecutek seed key from received seed bytes
 *
 * @return seed key (4 bytes)
 */
QByteArray FlashEcuSubaruDensoSH705xKline::subaru_denso_generate_ecutek_kline_seed_key(QByteArray requested_seed)
{
    QByteArray key;

    const uint16_t keytogenerateindex_1[]={
        0x53DA, 0x33BC, 0x72EB, 0x437D,
        0x7CA3, 0x3382, 0x834F, 0x3608,
        0xAFB8, 0x503D, 0xDBA3, 0x9D34,
        0x3563, 0x6B70, 0x6E74, 0x88F0
    };

    const uint16_t keytogenerateindex_2[]={
        0x24B9, 0x9D91, 0xFF0C, 0xB8D5,
        0x15BB, 0xF998, 0x8723, 0x9E05,
        0x7092, 0xD683, 0xBA03, 0x59E1,
        0x6136, 0x9B9A, 0x9CFB, 0x9DDB
    };

    const uint8_t indextransformation[]={
        0x4, 0x2, 0x5, 0x1, 0x8, 0xC, 0xD, 0x8,
        0xA, 0xD, 0x2, 0xB, 0xF, 0x4, 0x0, 0x3,
        0xB, 0x4, 0x6, 0x0, 0xF, 0x2, 0xD, 0x9,
        0x5, 0xC, 0x1, 0xA, 0x3, 0xD, 0xE, 0x8
    };

    key = subaru_denso_calculate_seed_key(requested_seed, keytogenerateindex_1, indextransformation);

    return key;
}

/*
 * Calculate denso seed key from received seed bytes
 *
 * @return seed key (4 bytes)
 */
QByteArray FlashEcuSubaruDensoSH705xKline::subaru_denso_calculate_seed_key(QByteArray requested_seed, const uint16_t *keytogenerateindex, const uint8_t *indextransformation)
{
    QByteArray key;

    uint32_t seed, index;
    uint16_t wordtogenerateindex, wordtobeencrypted, encryptionkey;
    int ki, n;

    seed = (requested_seed.at(0) << 24) & 0xFF000000;
    seed += (requested_seed.at(1) << 16) & 0x00FF0000;
    seed += (requested_seed.at(2) << 8) & 0x0000FF00;
    seed += requested_seed.at(3) & 0x000000FF;
    //seed = reconst_32(seed8);

    for (ki = 15; ki >= 0; ki--) {

        wordtogenerateindex = seed;
        wordtobeencrypted = seed >> 16;
        index = wordtogenerateindex ^ keytogenerateindex[ki];
        index += index << 16;
        encryptionkey = 0;

        for (n = 0; n < 4; n++) {
            encryptionkey += indextransformation[(index >> (n * 4)) & 0x1F] << (n * 4);
        }

        encryptionkey = (encryptionkey >> 3) + (encryptionkey << 13);
        seed = (encryptionkey ^ wordtobeencrypted) + (wordtogenerateindex << 16);
    }

    seed = (seed >> 16) + (seed << 16);

    key.clear();
    key.append((uint8_t)(seed >> 24));
    key.append((uint8_t)(seed >> 16));
    key.append((uint8_t)(seed >> 8));
    key.append((uint8_t)seed);

    //write_32b(seed, key);

    return key;
}

/*
 * Encrypt upload data
 *
 * @return encrypted data
 */
QByteArray FlashEcuSubaruDensoSH705xKline::subaru_denso_encrypt_32bit_payload(QByteArray buf, uint32_t len)
{
    QByteArray encrypted;

    uint16_t keytogenerateindex[]={
        0x7856, 0xCE22, 0xF513, 0x6E86
    };

    const uint8_t indextransformation[]={
        0x5, 0x6, 0x7, 0x1, 0x9, 0xC, 0xD, 0x8,
        0xA, 0xD, 0x2, 0xB, 0xF, 0x4, 0x0, 0x3,
        0xB, 0x4, 0x6, 0x0, 0xF, 0x2, 0xD, 0x9,
        0x5, 0xC, 0x1, 0xA, 0x3, 0xD, 0xE, 0x8
    };

    encrypted = subaru_denso_calculate_32bit_payload(buf, len, keytogenerateindex, indextransformation);

    return encrypted;
}

QByteArray FlashEcuSubaruDensoSH705xKline::subaru_denso_decrypt_32bit_payload(QByteArray buf, uint32_t len)
{
    QByteArray decrypted;

    uint16_t keytogenerateindex[]={
        0x6E86, 0xF513, 0xCE22, 0x7856
    };

    const uint8_t indextransformation[]={
        0x5, 0x6, 0x7, 0x1, 0x9, 0xC, 0xD, 0x8,
        0xA, 0xD, 0x2, 0xB, 0xF, 0x4, 0x0, 0x3,
        0xB, 0x4, 0x6, 0x0, 0xF, 0x2, 0xD, 0x9,
        0x5, 0xC, 0x1, 0xA, 0x3, 0xD, 0xE, 0x8
    };

    decrypted = subaru_denso_calculate_32bit_payload(buf, len, keytogenerateindex, indextransformation);

    return decrypted;
}

QByteArray FlashEcuSubaruDensoSH705xKline::subaru_denso_calculate_32bit_payload(QByteArray buf, uint32_t len, const uint16_t *keytogenerateindex, const uint8_t *indextransformation)
{
    QByteArray encrypted;
    uint32_t datatoencrypt32, index;
    uint16_t wordtogenerateindex, wordtobeencrypted, encryptionkey;
    int ki, n;

    if (!buf.length() || !len) {
        return NULL;
    }

    encrypted.clear();

    len &= ~3;
    for (uint32_t i = 0; i < len; i += 4) {
        datatoencrypt32 = ((buf.at(i) << 24) & 0xFF000000) | ((buf.at(i + 1) << 16) & 0xFF0000) | ((buf.at(i + 2) << 8) & 0xFF00) | (buf.at(i + 3) & 0xFF);

        for (ki = 0; ki < 4; ki++) {

            wordtogenerateindex = datatoencrypt32;
            wordtobeencrypted = datatoencrypt32 >> 16;
            index = wordtogenerateindex ^ keytogenerateindex[ki];
            index += index << 16;
            encryptionkey = 0;

            for (n = 0; n < 4; n++) {
                encryptionkey += indextransformation[(index >> (n * 4)) & 0x1F] << (n * 4);
            }

            encryptionkey = (encryptionkey >> 3) + (encryptionkey << 13);
            datatoencrypt32 = (encryptionkey ^ wordtobeencrypted) + (wordtogenerateindex << 16);
        }

        datatoencrypt32 = (datatoencrypt32 >> 16) + (datatoencrypt32 << 16);

        encrypted.append((datatoencrypt32 >> 24) & 0xFF);
        encrypted.append((datatoencrypt32 >> 16) & 0xFF);
        encrypted.append((datatoencrypt32 >> 8) & 0xFF);
        encrypted.append(datatoencrypt32 & 0xFF);
        //encrypted.append(sub_encrypt(tempbuf));
    }
    return encrypted;
}

/*
 * Request kernel init
 *
 * @return
 */
QByteArray FlashEcuSubaruDensoSH705xKline::request_kernel_init()
{
    QByteArray output;
    QByteArray received;

    request_denso_kernel_init = true;

    output.clear();
    output.append(SID_KERNEL_INIT);
    received = serial->write_serial_data_echo_check(output);
    delay(500);
    received = serial->read_serial_data(100, serial_read_short_timeout);

    request_denso_kernel_init = false;

    return received;
}

/*
 * Request kernel id
 *
 * @return kernel id
 */
QByteArray FlashEcuSubaruDensoSH705xKline::request_kernel_id()
{
    QByteArray output;
    QByteArray received;
    QByteArray msg;
    QByteArray kernelid;

    request_denso_kernel_id = true;

    output.clear();
    output.append(SID_RECUID);

    received = serial->write_serial_data_echo_check(output);
    delay(100);
    received = serial->read_serial_data(100, serial_read_short_timeout);

    received.remove(0, 1);
    kernelid = received;

    while (received != "")
    {
        received = serial->read_serial_data(1, serial_read_short_timeout);
        received.remove(0, 1);
        kernelid.append(received);
    }

    request_denso_kernel_id = false;

    return kernelid;
}
























/*
 * Add SSM header to message
 *
 * @return parsed message
 */
QByteArray FlashEcuSubaruDensoSH705xKline::add_ssm_header(QByteArray output, uint8_t tester_id, uint8_t target_id, bool dec_0x100)
{
    uint8_t length = output.length();

    output.insert(0, (uint8_t)0x80);
    output.insert(1, target_id & 0xFF);
    output.insert(2, tester_id & 0xFF);
    output.insert(3, length);

    output.append(calculate_checksum(output, dec_0x100));

    //emit LOG_I("Send: " + parse_message_to_hex(output), true, true);
    //qDebug () << "Send:" << parse_message_to_hex(output);
    return output;
}

/*
 * Calculate SSM checksum to message
 *
 * @return 8-bit checksum
 */
uint8_t FlashEcuSubaruDensoSH705xKline::calculate_checksum(QByteArray output, bool dec_0x100)
{
    uint8_t checksum = 0;

    for (uint16_t i = 0; i < output.length(); i++)
        checksum += (uint8_t)output.at(i);

    if (dec_0x100)
        checksum = (uint8_t) (0x100 - checksum);

    return checksum;
}

/*
 * Countdown prior power on
 *
 * @return
 */
int FlashEcuSubaruDensoSH705xKline::connect_bootloader_start_countdown(int timeout)
{
    for (int i = timeout; i > 0; i--)
    {
        if (kill_process)
            break;
        emit LOG_I("Starting in " + QString::number(i), true, true);
        //qDebug() << "Countdown:" << i;
        delay(1000);
    }
    if (!kill_process)
    {
        emit LOG_I("Initializing connection, please wait...", true, true);
        delay(750);
        return STATUS_SUCCESS;
    }

    return STATUS_ERROR;
}

/*
 * Parse QByteArray to readable form
 *
 * @return parsed message
 */
QString FlashEcuSubaruDensoSH705xKline::parse_message_to_hex(QByteArray received)
{
    QString msg;

    for (int i = 0; i < received.length(); i++)
    {
        msg.append(QString("%1 ").arg((uint8_t)received.at(i),2,16,QLatin1Char('0')).toUtf8());
    }

    return msg;
}

/*
 * Output text to log window
 *
 * @return
 */
int FlashEcuSubaruDensoSH705xKline::send_log_window_message(QString message, bool timestamp, bool linefeed)
{
    QDateTime dateTime = dateTime.currentDateTime();
    QString dateTimeString = dateTime.toString("[yyyy-MM-dd hh':'mm':'ss'.'zzz']  ");

    if (timestamp)
        message = dateTimeString + message;
    if (linefeed)
        message = message + "\n";

    QTextEdit* textedit = this->findChild<QTextEdit*>("text_edit");
    if (textedit)
    {
        ui->text_edit->insertPlainText(message);
        ui->text_edit->ensureCursorVisible();

        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        return STATUS_SUCCESS;
    }

    return STATUS_ERROR;
}

void FlashEcuSubaruDensoSH705xKline::set_progressbar_value(int value)
{
    bool valueChanged = true;
    if (ui->progressbar)
    {
        valueChanged = ui->progressbar->value() != value;
        ui->progressbar->setValue(value);
    }
    if (valueChanged)
        emit external_logger(value);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void FlashEcuSubaruDensoSH705xKline::delay(int timeout)
{
    QTime dieTime = QTime::currentTime().addMSecs(timeout);
    while (QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
}
