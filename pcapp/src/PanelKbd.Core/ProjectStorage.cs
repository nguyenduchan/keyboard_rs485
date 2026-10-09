using System.Text.Json;
using System.Text.Json.Serialization;

namespace PanelKbd.Core;

public static class ProjectStorage
{
    public const string Extension = ".pkproj";

    static readonly JsonSerializerOptions Opt = new()
    {
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter() },
        ReadCommentHandling = JsonCommentHandling.Skip,
    };

    public static void Save(ProjectFile p, string path)
    {
        p.FormatVersion = ProjectFile.CurrentFormat;
        var tmp = path + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(p, Opt));
        File.Move(tmp, path, overwrite: true);
    }

    public static ProjectFile Load(string path)
    {
        var p = JsonSerializer.Deserialize<ProjectFile>(File.ReadAllText(path), Opt)
                ?? throw new InvalidDataException("File dự án rỗng");
        if (p.FormatVersion > ProjectFile.CurrentFormat)
            throw new InvalidDataException($"File dự án phiên bản {p.FormatVersion} mới hơn phần mềm (hỗ trợ ≤ {ProjectFile.CurrentFormat}). Hãy cập nhật PC app.");
        p.EnsureComplete();
        return p;
    }
}
