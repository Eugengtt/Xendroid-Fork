package xendroid.compose;

import android.content.Context;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import xendroid.hardware.ProcessorInfo;

// Created by xendroid on 2025/7/31.
// SPDX-License-Identifier: WTFPL
public class Application extends android.app.Application{
    static File get_app_data_dir(){
        return ctx.getExternalFilesDir("compose");
    }
    public static File get_internal_data_dir()
    {
        return new File(ctx.getApplicationInfo().dataDir,"compose");
    }
    //sdcardfs文件系统无法创建可执行文件，只能放在内部存储(ext4)
    public static File get_custom_driver_dir()
    {
        return new File(get_internal_data_dir(),"driver");
    }
    public static File get_default_config_file(){
        return new File(Application.get_app_data_dir(),"default_config.toml");
    }

    public static File get_global_config_file(){
        return new File(Application.get_app_data_dir(),"xenia-canary.config.toml");
    }

    public  static byte[] load_assets_file(Context ctx,String asset_file_path) {
        try {
            InputStream in = ctx.getAssets().open(asset_file_path);
            int size = in.available();
            byte[] buffer = new byte[size];
            in.read(buffer);
            in.close();
            return buffer;
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }
    static String load_default_config_str(Context ctx){
        return new String(Application.load_assets_file(
                ctx,"config/default_config.toml"));
    }

    public static File get_uri_info_list_file(){
        return new File(Application.get_app_data_dir(),"uri_info_list.json");
    }

    public static File get_virtual_control_config_file(){
        return new File(Application.get_app_data_dir(),"virtual_control_config.json");
    }
    static boolean device_support_vulkan() {
        return gpu_device_name_vk!=null;
    }

    static boolean should_delay_load() {
        if(gpu_device_name_vk==null)
            throw new RuntimeException("gpu_device_name_vk==null");
        return gpu_device_name_vk.contains("Adreno (TM) 5")
                || gpu_device_name_vk.contains("Adreno (TM) 6");
    }

    public  static Context ctx;
    public static String gpu_device_name_vk;
    @Override
    public void onCreate()
    {
        super.onCreate();

        Application.ctx=this;
        gpu_device_name_vk= ProcessorInfo.gpu_get_physical_device_name_vk();

        String[] entry={"cache","cache0","cache1",};
        for(String e:entry){
            File f=new File(get_app_data_dir(),e);
            f.mkdirs();
        }
        // Refresh, not just create: it is a cache of the bundled asset, and the
        // settings UI diffs against it to decide what is modified from default.
        File default_config_file=get_default_config_file();
        String bundled_config=load_default_config_str(this);
        if(!default_config_file.exists()
                || !bundled_config.equals(Utils.load_string(default_config_file)))
            Utils.save_string(default_config_file,bundled_config);

        if(!should_delay_load())
            Emulator.load_library();

    }

}
