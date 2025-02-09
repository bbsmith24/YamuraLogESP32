using System.Text;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.Rebar;

namespace ESP32_CAN_reader
{
    public partial class Form1 : Form
    {
        GPS_Data gpsData = new GPS_Data();
        IMU_Data imuData = new IMU_Data();  
        AD_Data adData = new AD_Data();
        long adStart = 0;
        long imuStart = 0;
        long gpsStart = 0;
        long startPos = 0;
        int nextAD = 0x10;
        int nextIMU = 0x20;
        int nextGPS = 0x30;
        StringBuilder adBytes = new StringBuilder();
        StringBuilder imuBytes = new StringBuilder();
        StringBuilder gpsBytes = new StringBuilder();

        public Form1()
        {
            InitializeComponent();
        }

        private void btn_openLog_Click(object sender, EventArgs e)
        {
            if(openFileDialog1.ShowDialog() != DialogResult.OK ) 
            {
                return; 
            }
            txtLogFileName.Text = openFileDialog1.FileName;
            StringBuilder outStr = new StringBuilder();
            FileStream m_filestr = File.OpenRead(openFileDialog1.FileName);
            Int32 canID = 0;
            UInt32 timeStamp = 0;
            byte[] canPayload = new byte[8];
            using (BinaryReader readLog = new BinaryReader(m_filestr))
            {
                // read XXXXXXXXXXXX at top of log
                canPayload = readLog.ReadBytes(4);
                canPayload = readLog.ReadBytes(8);

                while (readLog.BaseStream.Position < (readLog.BaseStream.Length - 12))
                {
                    startPos = readLog.BaseStream.Position;
                    canID = BitConverter.ToInt32(readLog.ReadBytes(4));
                    // A-D node: 10-11 consecutive
                    if (canID == nextAD)
                    {
                        ReadNode_AD(readLog, canID, ref outStr);
                    }
                    // IMU node: 20-21 consecutive
                    else if (canID == nextIMU)
                    {
                        ReadNode_IMU(readLog, canID, ref outStr);
                    }
                    // GPS node: 30-33 consecutive
                    else if (canID == nextGPS)
                    {
                        ReadNode_GPS(readLog, canID, ref outStr);
                    }
                    else
                    {
                        canPayload = readLog.ReadBytes(8);
                        for (int idx = 0; idx < 8; idx++)
                        {
                            outStr.AppendFormat("{0:X02}\t", canPayload[idx]);
                        }
                        outStr.AppendLine("MALFORMED");
                    }
                }
            }
            txtLogFileContent.Text = outStr.ToString();
        }
        /// <summary>
        /// read AD node
        /// this is going to be more complicated than this
        ///     1. can have multiple AD nodes in CAN bus, need to differentiate them
        ///     2. can mix analog and digital values differently? depends on final design of the box, maybe to 4 analog, 8 digital only...
        /// </summary>
        /// <param name="readLog"></param>
        /// <param name="canID"></param>
        /// <param name="outStr"></param>
        void ReadNode_AD(BinaryReader readLog, int canID, ref StringBuilder outStr)
        {
            // test box is 4 analog, 4 digital
            // 
            // 0x10 timestamp (4 bytes) a1 (2 bytes) a2 (2 bytes)
            // 0x11 a3 (2 bytes) a4 (2 bytes) digital (1 byte) (3 bytes unused)
            // 
            // analog in range 0 - 4096 (2^12)
            // digital 1 bit per channel in a byte
            byte[] canPayload = new byte[8];
            // read data
            canPayload = readLog.ReadBytes(8);
            if (canID == 0x10)
            {
                adBytes.Clear();
            }
            for (int idx = 0; idx < 8; idx++)
            {
                adBytes.AppendFormat("{0:X02} ", canPayload[idx]);
            }
            if (canID == 0x10)
            {
                adStart = startPos;
                adData.timeStamp = canPayload[3] << 24;
                adData.timeStamp += canPayload[2] << 16;
                adData.timeStamp += canPayload[1] << 8;
                adData.timeStamp += canPayload[0];
                adData.analogValues[0] = (Int16)(canPayload[5] << 8);
                adData.analogValues[0] += canPayload[4];
                adData.analogValues[1] = (Int16)(canPayload[7] << 8);
                adData.analogValues[1] += canPayload[6];
                nextAD = 0x11;
            }
            else if (canID == 0x11)
            {
                adData.analogValues[2] = (Int16)(canPayload[1] << 8);
                adData.analogValues[2] += canPayload[0];
                adData.analogValues[3] = (Int16)(canPayload[3] << 8);
                adData.analogValues[3] += canPayload[2];
                adData.digitalValues = canPayload[4];
                outStr.AppendFormat("[{0:X08}]\t{1}\tAD\tanalog\t{2}\t{3}\t{4}\t{5}\tdigital\t{6}\t{7}\t{8}\t{9}\tdata {10}{11}",
                    adStart,
                    adData.timeStamp,
                    adData.analogValues[0],
                    adData.analogValues[1],
                    adData.analogValues[2],
                    adData.analogValues[3],
                    (adData.digitalValues & 0x1) > 0 ? "OFF" : "ON",
                    (adData.digitalValues & 0x2) > 0 ? "OFF" : "ON",
                    (adData.digitalValues & 0x4) > 0 ? "OFF" : "ON",
                    (adData.digitalValues & 0x8) > 0 ? "OFF" : "ON",
                    adBytes.ToString(),
                    Environment.NewLine);
                nextAD = 0x10;
            }
        }
        void ReadNode_IMU(BinaryReader readLog, int canID, ref StringBuilder outStr)
        {
            byte[] canPayload = new byte[8];
            // read data
            canPayload = readLog.ReadBytes(8);
            if (canID == 0x20)
            {
                imuBytes.Clear();
            }
            for (int idx = 0; idx < 8; idx++)
            {
                imuBytes.AppendFormat("{0:X02} ", canPayload[idx]);
            }
            if (canID == 0x20)
            {
                imuStart = startPos;
                imuData.timeStamp = canPayload[3] << 24;
                imuData.timeStamp += canPayload[2] << 16;
                imuData.timeStamp += canPayload[1] << 8;
                imuData.timeStamp += canPayload[0];
                imuData.aX = 0;
                byte[] floatBytes = new byte[4];
                floatBytes[0] = canPayload[4];
                floatBytes[1] = canPayload[5];
                floatBytes[2] = canPayload[6];
                floatBytes[3] = canPayload[7];
                imuData.aX = BitConverter.ToSingle(floatBytes, 0);
                nextIMU = 0x21;
            }
            else if (canID == 0x21)
            {
                byte[] floatBytes = new byte[4];
                floatBytes[0] = canPayload[0];
                floatBytes[1] = canPayload[1];
                floatBytes[2] = canPayload[2];
                floatBytes[3] = canPayload[3];
                imuData.aY = BitConverter.ToSingle(floatBytes, 0);
                floatBytes[0] = canPayload[4];
                floatBytes[1] = canPayload[5];
                floatBytes[2] = canPayload[6];
                floatBytes[3] = canPayload[7];
                imuData.aZ = BitConverter.ToSingle(floatBytes, 0);
                outStr.AppendFormat("[{0:X08}]\t{1}\tIMU\t{2} ax\t{3} ay\t{4} az\tdata {5}{6}",
                    imuStart,
                    imuData.timeStamp,
                    imuData.aX.ToString("0.0000"),
                    imuData.aY.ToString("0.0000"),
                    imuData.aZ.ToString("0.0000"),
                    imuBytes.ToString(),
                    Environment.NewLine);
                nextIMU = 0x20;
            }
        }
        void ReadNode_GPS(BinaryReader readLog, int canID, ref StringBuilder outStr)
        {
            byte[] canPayload = new byte[8];
            // read data
            canPayload = readLog.ReadBytes(8);
            if (canID == 0x30)
            {
                gpsBytes.Clear();
            }
            for (int idx = 0; idx < 8; idx++)
            {
                gpsBytes.AppendFormat("{0:X02} ", canPayload[idx]);
            }
            if (canID == 0x30)
            {
                gpsStart = startPos;
                gpsData.timeStamp =  canPayload[3] << 24;
                gpsData.timeStamp += canPayload[2] << 16;
                gpsData.timeStamp += canPayload[1] << 8;
                gpsData.timeStamp += canPayload[0];

                gpsData.year =  canPayload[5] << 8;
                gpsData.year += canPayload[4];
                gpsData.month = canPayload[6];
                gpsData.day = canPayload[7];
                nextGPS = 0x31;
            }
            else if (canID == 0x31)
            {
                gpsData.hour = canPayload[0];
                gpsData.minute = canPayload[1];
                gpsData.second = canPayload[2];
                gpsData.intLatitude  = canPayload[6] << 24;
                gpsData.intLatitude += canPayload[5] << 16;
                gpsData.intLatitude += canPayload[4] << 8;
                gpsData.intLatitude +=  canPayload[3];

                gpsData.intLongitude = canPayload[7];
                nextGPS = 0x32;
            }
            else if (canID == 0x32)
            {
                gpsData.intLongitude += canPayload[2] << 24;
                gpsData.intLongitude += canPayload[1] << 16;
                gpsData.intLongitude += canPayload[0] <<  8;

                gpsData.intCourse =  canPayload[6] << 24;
                gpsData.intCourse += canPayload[5] << 16;
                gpsData.intCourse += canPayload[4] <<  8;
                gpsData.intCourse += canPayload[3];
                
                gpsData.intSpeed = canPayload[7];
                nextGPS = 0x33;
            }
            else if (canID == 0x33)
            {
                gpsData.intSpeed += canPayload[2] << 24;
                gpsData.intSpeed += canPayload[1] << 16;
                gpsData.intSpeed += canPayload[0] <<  8;
                
                gpsData.satellitesInView = canPayload[3];
                
                gpsData.accuracy =  canPayload[7] << 24;
                gpsData.accuracy += canPayload[6] << 16;
                gpsData.accuracy += canPayload[5] <<  8;
                gpsData.accuracy += canPayload[4];

                gpsData.latitude = (Single)gpsData.intLatitude / (Single)10000000.0;
                gpsData.longitude = (Single)gpsData.intLongitude / (Single)10000000.0;
                gpsData.course = (Single)gpsData.intCourse / (Single)100000.0;
                gpsData.speed = (Single)gpsData.intSpeed; // reported in mm/s
                outStr.AppendFormat("[{0:X08}]\t{1}\tGPS\t{2}/{3}/{4}\t{5}:{6}:{7}\tLat {8}\tLong {9}\tCourse {10} deg\tSpeed {11} mm/s\tSIV {12}\tdata {13}{14}",
                    gpsStart,
                    gpsData.timeStamp,
                    gpsData.month,
                    gpsData.day,
                    gpsData.year,
                    gpsData.hour,
                    gpsData.minute,
                    gpsData.second,
                    gpsData.latitude,
                    gpsData.longitude,
                    gpsData.course,
                    gpsData.speed,
                    gpsData.satellitesInView,   
                    gpsBytes.ToString(),
                    Environment.NewLine);
                nextGPS = 0x30;
            }
        }
    }
    public class AD_Data
    {
        public Int32 timeStamp;
        public Int16[] analogValues = new Int16[4];
        public byte digitalValues = 0;
        public AD_Data() { }
    }
    public class IMU_Data
    {
        public Int32 timeStamp;
        // not all may be used, depending on setup and/or device
        public Single aX; // accelerometer X
        public Single aY; // accelerometer Y
        public Single aZ; // accelerometer Z
        public Single gX; // gyro X
        public Single gY; // gyro Y
        public Single gZ; // gyro Z
        public Single mX; // magnetometer X
        public Single mY; // magnetometer Y
        public Single mZ; // magnetometer Z
        public IMU_Data() { }
    }
    public class GPS_Data
    {
        public Int32 timeStamp;
        public Int32 year;
        public Int32 month;
        public Int32 day;
        public Int32 hour;
        public Int32 minute;
        public Int32 second;
        public Single latitude;
        public Single longitude;
        public Single course;
        public Single speed;
        public Int32 satellitesInView;
        public Int32 accuracy;
        public Int32 intLatitude;
        public Int32 intLongitude;
        public Int32 intCourse;
        public Int32 intSpeed;

        public GPS_Data() { }  
    }
}
