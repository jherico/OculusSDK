package com.oculusvr.capi;

import java.util.Arrays;
import java.util.List;

import org.saintandreas.math.Quaternion;

import com.sun.jna.Pointer;
import com.sun.jna.Structure;

public class Posef extends Structure {
  public Quatf Orientation;
  public Vector3f Position;

  public Posef() {
    super();
  }

  @Override
  protected List<?> getFieldOrder() {
    return Arrays.asList("Orientation", "Position");
  }

  public Posef(Quatf Orientation, Vector3f Position) {
    super();
    this.Orientation = Orientation;
    this.Position = Position;
  }

  public Posef(Pointer peer) {
    super(peer);
  }

  public static class ByReference extends Posef implements Structure.ByReference {

  };

  public static class ByValue extends Posef implements Structure.ByValue {

  };

  public org.saintandreas.math.Matrix4f toMatrix4f() {
    org.saintandreas.math.Vector3f p = Position.toVector3f();
    Quaternion o = Orientation.toQuaternion();
    return new org.saintandreas.math.Matrix4f().rotate(o).mult(new org.saintandreas.math.Matrix4f().translate(p));
  }
}
