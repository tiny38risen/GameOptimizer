using System.Drawing;
using System.Windows.Forms;

namespace GameOptimizer.UI;

public static class DesignSystem
{
    public static readonly Color BgColor = Color.FromArgb(18, 20, 24);
    public static readonly Color SurfaceColor = Color.FromArgb(32, 36, 43);
    public static readonly Color BorderColor = Color.FromArgb(56, 62, 74);

    public static readonly Color PrimaryColor = Color.FromArgb(0, 255, 204);
    public static readonly Color PrimaryHoverColor = Color.FromArgb(0, 204, 163);

    public static readonly Color TextTitle = Color.FromArgb(243, 244, 246);
    public static readonly Color TextBody = Color.FromArgb(209, 213, 219);
    public static readonly Color TextMuted = Color.FromArgb(107, 114, 128);

    public static readonly Color Success = Color.FromArgb(57, 255, 20);
    public static readonly Color Warning = Color.FromArgb(255, 234, 0);
    public static readonly Color Danger = Color.FromArgb(255, 0, 85);

    private const string MainFontFamily = "Malgun Gothic";
    public static readonly Font FontTitle = new("Segoe UI", 16F, FontStyle.Bold);
    public static readonly Font FontHeading = new(MainFontFamily, 11F, FontStyle.Bold);
    public static readonly Font FontBody = new(MainFontFamily, 9.75F, FontStyle.Regular);
    public static readonly Font FontSmall = new(MainFontFamily, 8.5F, FontStyle.Regular);

    public static readonly Padding CardPadding = new(20);
    public const int ControlMargin = 12;
}
