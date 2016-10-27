﻿namespace OGE.Core.GSF.Diagnostics.UI
{
    partial class FrmErrorFilterText
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
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
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.TxtErrorName = new System.Windows.Forms.TextBox();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.rdoRegex = new System.Windows.Forms.RadioButton();
            this.rdoContains = new System.Windows.Forms.RadioButton();
            this.RdoStartsWith = new System.Windows.Forms.RadioButton();
            this.BtnDone = new System.Windows.Forms.Button();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.groupBox1.SuspendLayout();
            this.SuspendLayout();
            // 
            // TxtErrorName
            // 
            this.TxtErrorName.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TxtErrorName.Location = new System.Drawing.Point(0, 0);
            this.TxtErrorName.Multiline = true;
            this.TxtErrorName.Name = "TxtErrorName";
            this.TxtErrorName.Size = new System.Drawing.Size(722, 352);
            this.TxtErrorName.TabIndex = 0;
            this.TxtErrorName.TextChanged += new System.EventHandler(this.TxtErrorName_TextChanged);
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            this.splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.TxtErrorName);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.groupBox1);
            this.splitContainer1.Panel2.Controls.Add(this.BtnDone);
            this.splitContainer1.Size = new System.Drawing.Size(722, 433);
            this.splitContainer1.SplitterDistance = 352;
            this.splitContainer1.TabIndex = 1;
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.rdoRegex);
            this.groupBox1.Controls.Add(this.rdoContains);
            this.groupBox1.Controls.Add(this.RdoStartsWith);
            this.groupBox1.Location = new System.Drawing.Point(25, 16);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(305, 49);
            this.groupBox1.TabIndex = 4;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Match Method";
            // 
            // rdoRegex
            // 
            this.rdoRegex.AutoSize = true;
            this.rdoRegex.Location = new System.Drawing.Point(205, 19);
            this.rdoRegex.Name = "rdoRegex";
            this.rdoRegex.Size = new System.Drawing.Size(67, 17);
            this.rdoRegex.TabIndex = 2;
            this.rdoRegex.Text = "Is Regex";
            this.rdoRegex.UseVisualStyleBackColor = true;
            // 
            // rdoContains
            // 
            this.rdoContains.AutoSize = true;
            this.rdoContains.Location = new System.Drawing.Point(120, 19);
            this.rdoContains.Name = "rdoContains";
            this.rdoContains.Size = new System.Drawing.Size(66, 17);
            this.rdoContains.TabIndex = 1;
            this.rdoContains.Text = "Contains";
            this.rdoContains.UseVisualStyleBackColor = true;
            // 
            // RdoStartsWith
            // 
            this.RdoStartsWith.AutoSize = true;
            this.RdoStartsWith.Checked = true;
            this.RdoStartsWith.Location = new System.Drawing.Point(15, 19);
            this.RdoStartsWith.Name = "RdoStartsWith";
            this.RdoStartsWith.Size = new System.Drawing.Size(77, 17);
            this.RdoStartsWith.TabIndex = 0;
            this.RdoStartsWith.TabStop = true;
            this.RdoStartsWith.Text = "Starts With";
            this.RdoStartsWith.UseVisualStyleBackColor = true;
            // 
            // BtnDone
            // 
            this.BtnDone.Location = new System.Drawing.Point(635, 16);
            this.BtnDone.Name = "BtnDone";
            this.BtnDone.Size = new System.Drawing.Size(75, 23);
            this.BtnDone.TabIndex = 3;
            this.BtnDone.Text = "Done";
            this.BtnDone.UseVisualStyleBackColor = true;
            this.BtnDone.Click += new System.EventHandler(this.BtnDone_Click);
            // 
            // FrmErrorFilterText
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(722, 433);
            this.Controls.Add(this.splitContainer1);
            this.Name = "FrmErrorFilterText";
            this.Text = "Text Filter";
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel1.PerformLayout();
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TextBox TxtErrorName;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Button BtnDone;
        internal System.Windows.Forms.RadioButton rdoRegex;
        internal System.Windows.Forms.RadioButton rdoContains;
        internal System.Windows.Forms.RadioButton RdoStartsWith;
    }
}