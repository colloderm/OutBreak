param(
    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [Parameter(Mandatory = $true)]
    [string]$MotionKeyframePath,

    [Parameter(Mandatory = $true)]
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'

$sourceFull = (Resolve-Path -LiteralPath $SourcePath).Path
$keyframeFull = (Resolve-Path -LiteralPath $MotionKeyframePath).Path
$outputFull = [System.IO.Path]::GetFullPath($OutputRoot)
$framesFull = Join-Path $outputFull 'Frames'

New-Item -ItemType Directory -Path $outputFull, $framesFull -Force | Out-Null

$typeDefinition = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class MainMenuFlipbookGenerator
{
    private static double Clamp01(double value)
    {
        if (value < 0.0) return 0.0;
        if (value > 1.0) return 1.0;
        return value;
    }

    private static double SmoothStep(double edge0, double edge1, double value)
    {
        if (edge0 == edge1) return value < edge0 ? 0.0 : 1.0;
        double t = Clamp01((value - edge0) / (edge1 - edge0));
        return t * t * (3.0 - 2.0 * t);
    }

    private static Bitmap ToArgb(Bitmap input)
    {
        Bitmap result = new Bitmap(input.Width, input.Height, PixelFormat.Format32bppArgb);
        using (Graphics graphics = Graphics.FromImage(result))
        {
            graphics.CompositingMode = CompositingMode.SourceCopy;
            graphics.DrawImage(input, 0, 0, input.Width, input.Height);
        }
        return result;
    }

    private static byte[] ReadPixels(Bitmap bitmap)
    {
        Rectangle rect = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
        BitmapData data = bitmap.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            int length = Math.Abs(data.Stride) * bitmap.Height;
            byte[] pixels = new byte[length];
            Marshal.Copy(data.Scan0, pixels, 0, length);
            return pixels;
        }
        finally
        {
            bitmap.UnlockBits(data);
        }
    }

    private static void WritePixels(Bitmap bitmap, byte[] pixels)
    {
        Rectangle rect = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
        BitmapData data = bitmap.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try
        {
            Marshal.Copy(pixels, 0, data.Scan0, pixels.Length);
        }
        finally
        {
            bitmap.UnlockBits(data);
        }
    }

    private static double EllipseMask(int x, int y, double cx, double cy, double rx, double ry)
    {
        double nx = (x - cx) / rx;
        double ny = (y - cy) / ry;
        double distanceSquared = nx * nx + ny * ny;
        return 1.0 - SmoothStep(0.48, 1.02, distanceSquared);
    }

    private static double SkyBoundary(int x)
    {
        int[] xs = { 305, 350, 390, 430, 470, 520, 570, 620, 690, 760, 830, 900, 970, 1040, 1120, 1200, 1280, 1360, 1440, 1520, 1600 };
        int[] ys = {  -5,  55, 100, 125, 155, 175, 215, 260, 285, 300, 305, 285, 280, 250, 255, 235, 185, 135,  90,  45,  -5 };

        if (x < xs[0] || x > xs[xs.Length - 1]) return -100.0;
        for (int i = 0; i < xs.Length - 1; i++)
        {
            if (x <= xs[i + 1])
            {
                double t = (x - xs[i]) / (double)(xs[i + 1] - xs[i]);
                return ys[i] + (ys[i + 1] - ys[i]) * t;
            }
        }
        return -100.0;
    }

    private static void SampleBilinear(byte[] pixels, int width, int height, int stride, double x, double y, out double b, out double g, out double r)
    {
        if (x < 0.0) x = 0.0;
        if (y < 0.0) y = 0.0;
        if (x > width - 1.001) x = width - 1.001;
        if (y > height - 1.001) y = height - 1.001;

        int x0 = (int)x;
        int y0 = (int)y;
        int x1 = Math.Min(x0 + 1, width - 1);
        int y1 = Math.Min(y0 + 1, height - 1);
        double tx = x - x0;
        double ty = y - y0;

        int p00 = y0 * stride + x0 * 4;
        int p10 = y0 * stride + x1 * 4;
        int p01 = y1 * stride + x0 * 4;
        int p11 = y1 * stride + x1 * 4;

        double b0 = pixels[p00] + (pixels[p10] - pixels[p00]) * tx;
        double g0 = pixels[p00 + 1] + (pixels[p10 + 1] - pixels[p00 + 1]) * tx;
        double r0 = pixels[p00 + 2] + (pixels[p10 + 2] - pixels[p00 + 2]) * tx;
        double b1 = pixels[p01] + (pixels[p11] - pixels[p01]) * tx;
        double g1 = pixels[p01 + 1] + (pixels[p11 + 1] - pixels[p01 + 1]) * tx;
        double r1 = pixels[p01 + 2] + (pixels[p11 + 2] - pixels[p01 + 2]) * tx;

        b = b0 + (b1 - b0) * ty;
        g = g0 + (g1 - g0) * ty;
        r = r0 + (r1 - r0) * ty;
    }

    private static byte ToByte(double value)
    {
        if (value <= 0.0) return 0;
        if (value >= 255.0) return 255;
        return (byte)(value + 0.5);
    }

    public static void Generate(string sourcePath, string keyframePath, string outputRoot)
    {
        const int frameCount = 60;
        string framesDirectory = Path.Combine(outputRoot, "Frames");
        Directory.CreateDirectory(framesDirectory);

        using (Bitmap sourceInput = new Bitmap(sourcePath))
        using (Bitmap keyframeInput = new Bitmap(keyframePath))
        {
            if (sourceInput.Width != keyframeInput.Width || sourceInput.Height != keyframeInput.Height)
                throw new InvalidOperationException("The source and motion keyframe dimensions must match.");

            using (Bitmap source = ToArgb(sourceInput))
            using (Bitmap keyframe = ToArgb(keyframeInput))
            {
                int width = source.Width;
                int height = source.Height;
                int stride = width * 4;
                byte[] sourcePixels = ReadPixels(source);
                byte[] keyframePixels = ReadPixels(keyframe);
                float[] skyMask = new float[width * height];
                float[] vegetationMask = new float[width * height];
                float[] roadMask = new float[width * height];
                float[] mistMask = new float[width * height];

                for (int y = 0; y < height; y++)
                {
                    for (int x = 0; x < width; x++)
                    {
                        int index = y * width + x;
                        int pixel = y * stride + x * 4;
                        double b = sourcePixels[pixel];
                        double g = sourcePixels[pixel + 1];
                        double r = sourcePixels[pixel + 2];
                        double luminance = 0.0722 * b + 0.7152 * g + 0.2126 * r;

                        double boundary = SkyBoundary(x);
                        double sky = Clamp01((boundary + 10.0 - y) / 28.0);
                        skyMask[index] = (float)sky;

                        double left = EllipseMask(x, y, 210, 570, 335, 300);
                        double right = EllipseMask(x, y, width - 175, 590, 330, 320);
                        double foreground = EllipseMask(x, y, width * 0.51, height - 78, width * 0.43, 190);
                        double vegetationRegion = Math.Max(left, Math.Max(right, foreground));
                        double vegetationColor = SmoothStep(0.5, 14.0, Math.Max(r, g) - b);
                        vegetationColor *= 1.0 - SmoothStep(105.0, 155.0, luminance);
                        vegetationMask[index] = (float)(vegetationRegion * vegetationColor * 0.86);

                        double roadCenter = width * 0.52;
                        double roadWidth = 115.0 + Math.Max(0.0, y - 550.0) * 1.35;
                        double roadRegion = SmoothStep(roadWidth + 28.0, roadWidth - 18.0, Math.Abs(x - roadCenter));
                        roadRegion *= SmoothStep(555.0, 690.0, y);
                        double wetTone = SmoothStep(38.0, 105.0, luminance);
                        double chroma = Math.Max(r, Math.Max(g, b)) - Math.Min(r, Math.Min(g, b));
                        wetTone *= 1.0 - SmoothStep(16.0, 65.0, chroma);
                        roadMask[index] = (float)(roadRegion * wetTone);

                        double mx = (x - width * 0.54) / (width * 0.30);
                        double my = (y - height * 0.64) / (height * 0.105);
                        double mist = Math.Exp(-(mx * mx + my * my) * 1.35);
                        mist *= 1.0 - 0.55 * vegetationRegion;
                        mistMask[index] = (float)mist;
                    }
                }

                for (int frameIndex = 0; frameIndex < frameCount; frameIndex++)
                {
                    string framePath = Path.Combine(framesDirectory, String.Format("Frame_{0:D3}.png", frameIndex));
                    if (frameIndex == 0)
                    {
                        File.Copy(sourcePath, framePath, true);
                        continue;
                    }

                    double phase = 2.0 * Math.PI * frameIndex / frameCount;
                    double sin1 = Math.Sin(phase);
                    double sin2 = Math.Sin(phase * 2.0);
                    double sin3 = Math.Sin(phase * 3.0);
                    double envelope = 0.5 - 0.5 * Math.Cos(phase);
                    double skyOffset = 7.0 * sin1 + 2.0 * sin2;
                    double foliageWind = 0.78 * sin1 + 0.22 * sin3;
                    double alternateBlend = 0.24 * envelope;
                    byte[] outputPixels = new byte[sourcePixels.Length];

                    for (int y = 0; y < height; y++)
                    {
                        for (int x = 0; x < width; x++)
                        {
                            int maskIndex = y * width + x;
                            int pixel = y * stride + x * 4;
                            double outB = sourcePixels[pixel];
                            double outG = sourcePixels[pixel + 1];
                            double outR = sourcePixels[pixel + 2];

                            double sky = skyMask[maskIndex];
                            if (sky > 0.001)
                            {
                                double sb, sg, sr, kb, kg, kr;
                                SampleBilinear(sourcePixels, width, height, stride, x - skyOffset, y + 0.35 * sin2, out sb, out sg, out sr);
                                SampleBilinear(keyframePixels, width, height, stride, x - skyOffset * 0.55, y, out kb, out kg, out kr);
                                double cloudB = sb + (kb - sb) * alternateBlend;
                                double cloudG = sg + (kg - sg) * alternateBlend;
                                double cloudR = sr + (kr - sr) * alternateBlend;
                                outB += (cloudB - outB) * sky;
                                outG += (cloudG - outG) * sky;
                                outR += (cloudR - outR) * sky;
                            }

                            double vegetation = vegetationMask[maskIndex];
                            if (vegetation > 0.001)
                            {
                                double heightFactor = 0.6 + 0.9 * (1.0 - y / (double)height);
                                double sway = foliageWind * 5.2 * heightFactor;
                                double vb, vg, vr;
                                SampleBilinear(sourcePixels, width, height, stride, x - sway, y + Math.Abs(sway) * 0.10, out vb, out vg, out vr);
                                double vegetationStrength = vegetation * 0.72;
                                outB += (vb - outB) * vegetationStrength;
                                outG += (vg - outG) * vegetationStrength;
                                outR += (vr - outR) * vegetationStrength;
                            }

                            double road = roadMask[maskIndex];
                            if (road > 0.001)
                            {
                                double ripple = Math.Sin(x * 0.105 + y * 0.051 + phase * 2.0);
                                ripple += 0.45 * Math.Sin(x * 0.043 - y * 0.087 - phase * 3.0);
                                double gain = 1.0 + road * envelope * ripple * 0.014;
                                outB *= gain;
                                outG *= gain;
                                outR *= gain;
                            }

                            double mist = mistMask[maskIndex];
                            if (mist > 0.001)
                            {
                                double mistNoise = 0.58 + 0.42 * Math.Sin(x * 0.011 + y * 0.017 - phase * 1.8);
                                double mistAlpha = mist * envelope * mistNoise * 0.024;
                                outB += (171.0 - outB) * mistAlpha;
                                outG += (169.0 - outG) * mistAlpha;
                                outR += (161.0 - outR) * mistAlpha;
                            }

                            outputPixels[pixel] = ToByte(outB);
                            outputPixels[pixel + 1] = ToByte(outG);
                            outputPixels[pixel + 2] = ToByte(outR);
                            outputPixels[pixel + 3] = 255;
                        }
                    }

                    using (Bitmap output = new Bitmap(width, height, PixelFormat.Format32bppArgb))
                    {
                        WritePixels(output, outputPixels);
                        output.Save(framePath, ImageFormat.Png);
                    }
                }

                CreateAtlas(framesDirectory, Path.Combine(outputRoot, "MainMenuBackground_Flipbook_8x8_836x471.png"), 836, 471, 8, 8, frameCount);
                CreatePreview(framesDirectory, Path.Combine(outputRoot, "MotionPreview_10Frames.jpg"), width, height);
            }
        }
    }

    private static void CreateAtlas(string framesDirectory, string outputPath, int cellWidth, int cellHeight, int columns, int rows, int frameCount)
    {
        using (Bitmap atlas = new Bitmap(cellWidth * columns, cellHeight * rows, PixelFormat.Format24bppRgb))
        using (Graphics graphics = Graphics.FromImage(atlas))
        {
            graphics.Clear(Color.Black);
            graphics.CompositingMode = CompositingMode.SourceCopy;
            graphics.CompositingQuality = CompositingQuality.HighQuality;
            graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;

            for (int i = 0; i < frameCount; i++)
            {
                string framePath = Path.Combine(framesDirectory, String.Format("Frame_{0:D3}.png", i));
                using (Bitmap frame = new Bitmap(framePath))
                {
                    int x = (i % columns) * cellWidth;
                    int y = (i / columns) * cellHeight;
                    graphics.DrawImage(frame, new Rectangle(x, y, cellWidth, cellHeight), 0, 0, frame.Width, frame.Height, GraphicsUnit.Pixel);
                }
            }
            atlas.Save(outputPath, ImageFormat.Png);
        }
    }

    private static void CreatePreview(string framesDirectory, string outputPath, int sourceWidth, int sourceHeight)
    {
        int thumbWidth = 334;
        int thumbHeight = (int)Math.Round(thumbWidth * sourceHeight / (double)sourceWidth);
        using (Bitmap preview = new Bitmap(thumbWidth * 5, thumbHeight * 2, PixelFormat.Format24bppRgb))
        using (Graphics graphics = Graphics.FromImage(preview))
        {
            graphics.Clear(Color.Black);
            graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            for (int i = 0; i < 10; i++)
            {
                int frameIndex = i * 6;
                string framePath = Path.Combine(framesDirectory, String.Format("Frame_{0:D3}.png", frameIndex));
                using (Bitmap frame = new Bitmap(framePath))
                {
                    int x = (i % 5) * thumbWidth;
                    int y = (i / 5) * thumbHeight;
                    graphics.DrawImage(frame, new Rectangle(x, y, thumbWidth, thumbHeight), 0, 0, frame.Width, frame.Height, GraphicsUnit.Pixel);
                }
            }

            ImageCodecInfo jpegCodec = null;
            foreach (ImageCodecInfo codec in ImageCodecInfo.GetImageEncoders())
            {
                if (codec.FormatID == ImageFormat.Jpeg.Guid)
                {
                    jpegCodec = codec;
                    break;
                }
            }
            if (jpegCodec == null)
            {
                preview.Save(outputPath, ImageFormat.Jpeg);
                return;
            }
            using (EncoderParameters parameters = new EncoderParameters(1))
            {
                parameters.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, 92L);
                preview.Save(outputPath, jpegCodec, parameters);
            }
        }
    }
}
'@

if (-not ('MainMenuFlipbookGenerator' -as [type])) {
    Add-Type -TypeDefinition $typeDefinition -ReferencedAssemblies 'System.Drawing'
}

[MainMenuFlipbookGenerator]::Generate($sourceFull, $keyframeFull, $outputFull)

Write-Output "Generated 60 frames, an 8x8 atlas, and a contact preview in: $outputFull"
