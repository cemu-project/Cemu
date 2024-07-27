package info.cemu.Cemu.utils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Paths;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class ZipUtil {

    public static void unzip(InputStream stream, String targetDir) throws IOException {
        try (ZipInputStream zipInputStream = new ZipInputStream(stream)) {
            ZipEntry zipEntry;
            byte[] buffer = new byte[8192];
            while ((zipEntry = zipInputStream.getNextEntry()) != null) {
                String filePath = Paths.get(targetDir, zipEntry.getName()).toString();
                if (zipEntry.isDirectory()) {
                    createDir(filePath);
                } else {
                    try (FileOutputStream fileOutputStream = new FileOutputStream(filePath)) {
                        int bytesRead;
                        while ((bytesRead = zipInputStream.read(buffer)) > 0) {
                            fileOutputStream.write(buffer, 0, bytesRead);
                        }
                    }
                }
                zipInputStream.closeEntry();
            }
        }
    }

    private static void createDir(String dir) {
        File f = new File(dir);
        if (!f.isDirectory()) {
            f.mkdirs();
        }
    }
}
