package com.external.amongusmod;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import androidx.core.app.NotificationCompat;

public class FloatingModService extends Service {

    static {
        System.loadLibrary("amongus_ext");
    }

    public native boolean attachToGame();
    public native String getConnectionInfo();
    public native void applyFeaturePatch(int featureId, boolean enabled, float value);

    private WindowManager windowManager;
    private LinearLayout rootLayout;
    private LinearLayout menuContainer;
    private TextView statusText;
    private boolean isExpanded = false;

    private final boolean[] activeFeatures = new boolean[16];
    private float currentSpeed = 1.0f;

    private final Handler watcherHandler = new Handler(Looper.getMainLooper());
    private final Runnable gameWatcherRunnable = new Runnable() {
        @Override
        public void run() {
            boolean attached = attachToGame();
            if (attached) {
                for (int i = 1; i <= 14; i++) {
                    if (activeFeatures[i]) applyFeaturePatch(i, true, 0f);
                }
                if (activeFeatures[15]) applyFeaturePatch(15, true, currentSpeed);
            }
            if (statusText != null) {
                statusText.setText(getConnectionInfo());
                statusText.setTextColor(attached ? 0xFF00E676 : 0xFFFFAB00);
            }
            watcherHandler.postDelayed(this, 2500);
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        startForegroundNotification();
        setupFloatingWindow();
        watcherHandler.post(gameWatcherRunnable);
    }

    private void startForegroundNotification() {
        String channelId = "amongus_vip_mod_channel";
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    channelId, "Among Us VIP Mod", NotificationManager.IMPORTANCE_LOW);
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) manager.createNotificationChannel(channel);
        }
        Notification notification = new NotificationCompat.Builder(this, channelId)
                .setContentTitle("Among Us VIP v4.0")
                .setContentText("PTRACE Bypass Aktif")
                .setSmallIcon(android.R.drawable.ic_menu_manage)
                .build();
        startForeground(1, notification);
    }

    private void setupFloatingWindow() {
        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        int layoutFlag = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        // FLAG_NOT_TOUCH_MODAL eklendi! Dokunma olaylarının içeri sızmasını engeller, kaydırmayı düzeltir.
        int flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL |
                    WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS;

        final WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                layoutFlag,
                flags,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 50;
        params.y = 100;

        rootLayout = new LinearLayout(this);
        rootLayout.setOrientation(LinearLayout.VERTICAL);

        Button toggleIcon = new Button(this);
        toggleIcon.setText("VIP");
        toggleIcon.setTextColor(Color.WHITE);
        toggleIcon.setTextSize(16);
        GradientDrawable btnBg = new GradientDrawable();
        btnBg.setColor(Color.parseColor("#FF5252"));
        btnBg.setCornerRadius(40f);
        toggleIcon.setBackground(btnBg);
        toggleIcon.setOnTouchListener(new View.OnTouchListener() {
            private int initialX, initialY;
            private float initialTouchX, initialTouchY;
            private boolean moved;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = params.x;
                        initialY = params.y;
                        initialTouchX = event.getRawX();
                        initialTouchY = event.getRawY();
                        moved = false;
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        int dx = (int) (event.getRawX() - initialTouchX);
                        int dy = (int) (event.getRawY() - initialTouchY);
                        if (Math.abs(dx) > 15 || Math.abs(dy) > 15) moved = true;
                        params.x = initialX + dx;
                        params.y = initialY + dy;
                        windowManager.updateViewLayout(rootLayout, params);
                        return true;
                    case MotionEvent.ACTION_UP:
                        if (!moved) toggleMenu();
                        return true;
                }
                return false;
            }
        });
        rootLayout.addView(toggleIcon);

        menuContainer = new LinearLayout(this);
        menuContainer.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable menuBg = new GradientDrawable();
        menuBg.setColor(Color.parseColor("#F20D1117"));
        menuBg.setCornerRadius(24f);
        menuContainer.setBackground(menuBg);
        menuContainer.setPadding(32, 32, 32, 32);
        menuContainer.setVisibility(View.GONE);

        TextView header = new TextView(this);
        header.setText("VIP MENÜ");
        header.setTextColor(Color.parseColor("#FF5252"));
        header.setTextSize(18);
        header.setGravity(Gravity.CENTER);
        header.setPadding(0, 0, 0, 12);
        menuContainer.addView(header);

        statusText = new TextView(this);
        statusText.setText("Bağlantı bekleniyor...");
        statusText.setTextSize(11);
        statusText.setPadding(0, 0, 0, 16);
        menuContainer.addView(statusText);

        // Yatay ve dikeyde tam kaydırma için ScrollView yapılandırması
        ScrollView scrollView = new ScrollView(this);
        scrollView.setLayoutParams(new LinearLayout.LayoutParams(
                (int)(getResources().getDisplayMetrics().widthPixels * 0.55f), // Genişlik
                (int) (getResources().getDisplayMetrics().heightPixels * 0.65f))); // Yükseklik
        scrollView.setVerticalScrollBarEnabled(true);
        scrollView.setFillViewport(false); // İçeriğin taşmasına izin ver

        LinearLayout featuresList = new LinearLayout(this);
        featuresList.setOrientation(LinearLayout.VERTICAL);
        featuresList.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        addSlider(featuresList);

        addToggle(featuresList, "1. No Kill Cooldown", 1);
        addToggle(featuresList, "2. Full Bright (Işık)", 2);
        addToggle(featuresList, "3. Always Move", 3);
        addToggle(featuresList, "4. Long Kill", 4);
        addToggle(featuresList, "5. Her Şeyi Kullan", 5);
        addToggle(featuresList, "6. Sınırsız Engineer", 6);
        addToggle(featuresList, "7. Acil Toplantı", 7);
        addToggle(featuresList, "8. Anti-Mantar/Comms", 8);
        addToggle(featuresList, "9. Impostor Rol", 9);
        addToggle(featuresList, "10. Merdiven Bekleme", 10);
        addToggle(featuresList, "11. Hayalet Görevleri", 11);
        addToggle(featuresList, "12. Impostor Onayı", 12);
        addToggle(featuresList, "13. Lobi Ban/Kick", 13);
        addToggle(featuresList, "14. Free Chat", 14);

        Button hideBtn = new Button(this);
        hideBtn.setText("KAPAT");
        hideBtn.setTextColor(Color.WHITE);
        GradientDrawable hideBg = new GradientDrawable();
        hideBg.setColor(Color.parseColor("#B71C1C"));
        hideBg.setCornerRadius(12f);
        hideBtn.setBackground(hideBg);
        hideBtn.setOnClickListener(v -> toggleMenu());
        featuresList.addView(hideBtn);

        Button closeServiceBtn = new Button(this);
        closeServiceBtn.setText("TAMAMEN KAPAT");
        closeServiceBtn.setTextColor(Color.WHITE);
        closeServiceBtn.setBackgroundColor(Color.TRANSPARENT);
        closeServiceBtn.setOnClickListener(v -> stopSelf());
        featuresList.addView(closeServiceBtn);

        scrollView.addView(featuresList);
        menuContainer.addView(scrollView);
        rootLayout.addView(menuContainer);

        windowManager.addView(rootLayout, params);
    }

    private void addSlider(LinearLayout parent) {
        final TextView speedLabel = new TextView(this);
        speedLabel.setText("15. Hız Çarpanı: 1.0x");
        speedLabel.setTextColor(Color.parseColor("#00E5FF"));
        speedLabel.setTextSize(12);
        speedLabel.setPadding(0, 8, 0, 4);
        parent.addView(speedLabel);

        SeekBar speedSeek = new SeekBar(this);
        speedSeek.setMax(10);
        speedSeek.setProgress(1);
        speedSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                currentSpeed = Math.max(1.0f, (float) progress);
                boolean en = progress > 1;
                activeFeatures[15] = en;
                speedLabel.setText("15. Hız Çarpanı: " + currentSpeed + "x [" + (en ? "AÇIK" : "KAPALI") + "]");
                applyFeaturePatch(15, en, currentSpeed);
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        parent.addView(speedSeek);
    }

    private void addToggle(LinearLayout parent, String title, final int featureId) {
        Switch sw = new Switch(this);
        sw.setText(title);
        sw.setTextColor(Color.parseColor("#E6EDF3"));
        sw.setTextSize(13);
        sw.setPadding(0, 16, 0, 16);
        sw.setOnCheckedChangeListener((buttonView, isChecked) -> {
            activeFeatures[featureId] = isChecked;
            applyFeaturePatch(featureId, isChecked, 0f);
        });
        parent.addView(sw);
    }

    private void toggleMenu() {
        isExpanded = !isExpanded;
        menuContainer.setVisibility(isExpanded ? View.VISIBLE : View.GONE);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        watcherHandler.removeCallbacks(gameWatcherRunnable);
        if (rootLayout != null && windowManager != null) {
            windowManager.removeView(rootLayout);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
