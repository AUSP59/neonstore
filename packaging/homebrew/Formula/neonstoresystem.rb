class Neonstoresystem < Formula
  desc "World-class C++ store management CLI and library"
  homepage "https://github.com/YOUR_ORG/neonstoresystem"
  url "https://github.com/YOUR_ORG/neonstoresystem/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "REPLACE_WITH_TARBALL_SHA256"
  license "Apache-2.0"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"
    system "cmake", "--build", "build", "-j"
    system "cmake", "--install", "build", "--prefix", prefix
  end

  test do
    system "#{bin}/neonstore", "--help"
  end
end
