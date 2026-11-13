package com.external.amongusmod;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    private static final int OVERLAY_PERMISSION_REQ_CODE = 1234;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setBackgroundColor(0xFF0D1117);
        layout.setPadding(48, 48, 48, 48);

        TextView title = new TextView(this);
        title.setText("AMONG US VIP v4.0");
        title.setTextColor(0xFFFF3B30);
        title.setTextSize(24);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        TextView desc = new TextView(this);
        desc.setText("\n• PTRACE Bypass Aktif\n• Android 15 & Yatay Ekran Uyumlu\n• Gelişmiş Dokunmatik Kontrol\n");
        desc.setTextColor(0xFF8B949E);
        desc.setTextSize(14);
        desc.setGravity(Gravity.CENTER);
        layout.addView(desc);

        Button startBtn = new Button(this);
        startBtn.setText("MENÜYÜ BAŞLAT");
        startBtn.setBackgroundColor(0xFFD32F2F);
        startBtn.setTextColor(0xFFFFFFFF);
        startBtn.setOnClickListener(v -> checkPermissionAndStart());
        layout.addView(startBtn);

        setContentView(layout);
    }

    private void checkPermissionAndStart() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !Settings.canDrawOverlays(this)) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            startActivityForResult(intent, OVERLAY_PERMISSION_REQ_CODE);
        } else {
            startModService();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == OVERLAY_PERMISSION_REQ_CODE) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && Settings.canDrawOverlays(this)) {
                startModService();
            } else {
                Toast.makeText(this, "Üstte gösterme izni gerekli!", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void startModService() {
        Intent serviceIntent = new Intent(this, FloatingModService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent);
        } else {
            startService(serviceIntent);
        }
        Toast.makeText(this, "VIP Mod Menü Başlatıldı!", Toast.LENGTH_SHORT).show();
        moveTaskToBack(true);
    }
}
