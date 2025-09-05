class Neonstore < Formula
  desc "Store management CLI and C++ library"
  homepage "https://example.org/neonstore"
  url "https://example.org/neonstore-2.12.0.tar.gz"
  sha256 "REPLACE_ME"
  license "Apache-2.0"
  depends_on "cmake" => :build
  def install
    system "cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"
    system "cmake", "--build", "build", "--parallel"
    system "cmake", "--install", "build", "--prefix", prefix
  end
  test do
    system "#{bin}/neonstore", "--version"
  end
end
