package com.oculusvr.capi;

import java.util.Arrays;
import java.util.List;

import org.saintandreas.math.Quaternion;

import com.sun.jna.Pointer;
import com.sun.jna.Structure;

public class Quatf extends Structure {
  public float x;
  public float y;
  public float z;
  public float w;

  public Quatf() {
    super();
  }

  @Override
  protected List<?> getFieldOrder() {
    return Arrays.asList("x", "y", "z", "w");
  }

  public Quatf(float x, float y, float z, float w) {
    super();
    this.x = x;
    this.y = y;
    this.z = z;
    this.w = w;
  }

  public Quatf(Pointer peer) {
    super(peer);
  }

  public static class ByReference extends Quatf implements Structure.ByReference {

  };

  public static class ByValue extends Quatf implements Structure.ByValue {

  };

  public Quaternion toQuaternion() {
    return new Quaternion(x, y, z, w);
  }
}
