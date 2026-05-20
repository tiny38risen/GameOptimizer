namespace GameOptimizer.UI;

public sealed partial class MainForm
{
    private void InitializeComponent()
    {
        SuspendLayout();
        Text = "GameOptimizer v3.0 운영 콘솔";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(1080, 720);
        Size = new Size(1180, 780);
        Font = new Font("Segoe UI", 10F);
        BuildLayout();
        ResumeLayout(false);
    }
}
