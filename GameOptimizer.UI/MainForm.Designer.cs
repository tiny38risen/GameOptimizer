namespace GameOptimizer.UI;

public sealed partial class MainForm
{
    private void InitializeComponent()
    {
        SuspendLayout();
        Text = "GameOptimizer v3.0 운영 콘솔";
        StartPosition = FormStartPosition.CenterScreen;
        AutoScroll = true;
        MinimumSize = new Size(1080, 720);
        Size = new Size(1180, 780);
        BackColor = DesignSystem.BgColor;
        ForeColor = DesignSystem.TextBody;
        Font = DesignSystem.FontBody;
        BuildLayout();
        ResumeLayout(false);
    }
}
