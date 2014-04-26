package com.oculusvr.capi;

import java.util.Arrays;
import java.util.List;

import com.oculusvr.capi.OvrLibrary.HDC;
import com.oculusvr.capi.OvrLibrary.HGLRC;
import com.oculusvr.capi.OvrLibrary.HWND;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

public class GLConfigData extends Structure {
  /** General device settings. */
  public RenderAPIConfigHeader Header;

  public GLConfigData() {
    super();
  }

  @Override
  protected List<?> getFieldOrder() {
    return Arrays.asList("Header");
  }

  public GLConfigData(RenderAPIConfigHeader Header, HWND Window, HGLRC WglContext, HDC GdiDc) {
    super();
    this.Header = Header;
  }

  public GLConfigData(Pointer peer) {
    super(peer);
  }

  public static class ByReference extends GLConfigData implements Structure.ByReference {

  };

  public static class ByValue extends GLConfigData implements Structure.ByValue {

  };
}
