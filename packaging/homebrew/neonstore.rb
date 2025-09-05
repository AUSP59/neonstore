class Neonstore < Formula
  desc "Store management CLI and C++ library"
  homepage "https://example.org/neonstore"
  url "https://example.org/neonstore-2.23.0.tar.gz"
  sha256 "CHANGE_ME_SHA256"
  license "Apache-2.0"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"
    system "cmake", "--build", "build", "--parallel"
    system "cmake", "--install", "build", "--prefix", prefix
    man1.install "man/neonstore.1"
  end

  test do
    system "#{bin}/neonstore", "--help"
  end
end
