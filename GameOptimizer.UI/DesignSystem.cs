using System.Drawing;
using System.Windows.Forms;

namespace GameOptimizer.UI;

public static class DesignSystem
{
    public static readonly Color BgColor = Color.FromArgb(245, 246, 248);
    public static readonly Color SurfaceColor = Color.White;
    public static readonly Color BorderColor = Color.FromArgb(229, 231, 235);

    public static readonly Color PrimaryColor = Color.RoyalBlue;
    public static readonly Color PrimaryHoverColor = Color.FromArgb(52, 87, 178);
    public static readonly Color SecondaryButtonColor = Color.WhiteSmoke;

    public static readonly Color TextTitle = Color.FromArgb(17, 24, 39);
    public static readonly Color TextBody = Color.Black;
    public static readonly Color TextMuted = Color.DimGray;

    public static readonly Color Success = Color.SeaGreen;
    public static readonly Color Warning = Color.DarkGoldenrod;
    public static readonly Color Danger = Color.Firebrick;

    private const string MainFontFamily = "Malgun Gothic";
    public static readonly Font FontTitle = new("Segoe UI", 18F, FontStyle.Bold);
    public static readonly Font FontHeading = new(MainFontFamily, 11F, FontStyle.Bold);
    public static readonly Font FontBody = new(MainFontFamily, 9.75F, FontStyle.Regular);
    public static readonly Font FontSmall = new(MainFontFamily, 8.5F, FontStyle.Regular);

    public static readonly Padding CardPadding = new(20);
    public const int ControlMargin = 12;
}
