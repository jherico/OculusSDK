package com.oculusvr.capi;

import java.util.Arrays;
import java.util.List;

import com.sun.jna.Pointer;
import com.sun.jna.Structure;

public class Vector3f extends Structure {
  public float x;
  public float y;
  public float z;

  public Vector3f() {
    super();
  }

  @Override
  protected List<?> getFieldOrder() {
    return Arrays.asList("x", "y", "z");
  }

  public Vector3f(float x, float y, float z) {
    super();
    this.x = x;
    this.y = y;
    this.z = z;
  }

  public Vector3f(Pointer peer) {
    super(peer);
  }

  public static class ByReference extends Vector3f implements Structure.ByReference {

  };

  public static class ByValue extends Vector3f implements Structure.ByValue {

  };

  public org.saintandreas.math.Vector3f toVector3f() {
    return new org.saintandreas.math.Vector3f(x, y, z);
  }
}
