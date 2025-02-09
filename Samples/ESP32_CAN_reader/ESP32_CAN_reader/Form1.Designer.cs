namespace ESP32_CAN_reader
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            openFileDialog1 = new OpenFileDialog();
            btn_openLog = new Button();
            txtLogFileName = new TextBox();
            txtLogFileContent = new TextBox();
            SuspendLayout();
            // 
            // openFileDialog1
            // 
            openFileDialog1.FileName = "openFileDialog1";
            // 
            // btn_openLog
            // 
            btn_openLog.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            btn_openLog.Location = new Point(1227, 12);
            btn_openLog.Name = "btn_openLog";
            btn_openLog.Size = new Size(75, 23);
            btn_openLog.TabIndex = 0;
            btn_openLog.Text = "Open LOG";
            btn_openLog.UseVisualStyleBackColor = true;
            btn_openLog.Click += btn_openLog_Click;
            // 
            // txtLogFileName
            // 
            txtLogFileName.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            txtLogFileName.Location = new Point(12, 13);
            txtLogFileName.Name = "txtLogFileName";
            txtLogFileName.Size = new Size(1209, 23);
            txtLogFileName.TabIndex = 1;
            // 
            // txtLogFileContent
            // 
            txtLogFileContent.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            txtLogFileContent.Font = new Font("Courier New", 9.75F, FontStyle.Regular, GraphicsUnit.Point, 0);
            txtLogFileContent.Location = new Point(12, 42);
            txtLogFileContent.Multiline = true;
            txtLogFileContent.Name = "txtLogFileContent";
            txtLogFileContent.ScrollBars = ScrollBars.Both;
            txtLogFileContent.Size = new Size(1290, 396);
            txtLogFileContent.TabIndex = 2;
            txtLogFileContent.WordWrap = false;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(1314, 450);
            Controls.Add(txtLogFileContent);
            Controls.Add(txtLogFileName);
            Controls.Add(btn_openLog);
            Name = "Form1";
            Text = "Form1";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private OpenFileDialog openFileDialog1;
        private Button btn_openLog;
        private TextBox txtLogFileName;
        private TextBox txtLogFileContent;
    }
}
