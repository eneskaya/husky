# -*- mode: ruby -*-
# vi: set ft=ruby :

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure("2") do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://vagrantcloud.com/search.
  config.vm.box = "ubuntu/trusty64"

  # Disable automatic box update checking. If you disable this, then
  # boxes will only be checked for updates when the user runs
  # `vagrant box outdated`. This is not recommended.
  # config.vm.box_check_update = false

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine. In the example below,
  # accessing "localhost:8080" will access port 80 on the guest machine.
  # NOTE: This will enable public access to the opened port
  # config.vm.network "forwarded_port", guest: 80, host: 8080

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine and only allow access
  # via 127.0.0.1 to disable public access
  # config.vm.network "forwarded_port", guest: 80, host: 8080, host_ip: "127.0.0.1"

  # Create a private network, which allows host-only access to the machine
  # using a specific IP.
  config.vm.network "private_network", ip: "192.168.33.10"

  # Create a public network, which generally matched to bridged network.
  # Bridged networks make the machine appear as another physical device on
  # your network.
  # config.vm.network "public_network"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  config.vm.synced_folder ".", "/husky", type: "nfs"

  # Provider-specific configuration so you can fine-tune various
  # backing providers for Vagrant. These expose provider-specific options.
  # Example for VirtualBox:
  #
  # config.vm.provider "virtualbox" do |vb|
  #   # Display the VirtualBox GUI when booting the machine
  #   vb.gui = true
  #
  #   # Customize the amount of memory on the VM:
  #   vb.memory = "1024"
  # end
  #
  # View the documentation for the provider you are using for more
  # information on available options.

  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  config.vm.provision "shell", inline: <<-SHELL
    apt-get -y update
    apt-get install -y software-properties-common
    add-apt-repository -y ppa:ubuntu-toolchain-r/test
    add-apt-repository -y ppa:kojoley/boost
    add-apt-repository -y ppa:george-edison55/cmake-3.x
    apt-get -y update
    apt-get install -y apt-transport-https build-essential \
      gcc-4.9 g++-4.9 cmake git
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 50
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.9 50
    apt-get install -y libboost-chrono1.58-dev libboost-program-options1.58-dev \
      libboost-thread1.58-dev libboost-filesystem1.58-dev libboost-regex1.58-dev
    apt-get install -y libgoogle-perftools-dev libzmq3-dev libprotobuf8
    echo "deb https://dl.bintray.com/wangzw/deb trusty contrib" | tee /etc/apt/sources.list.d/bintray-wangzw-deb.list
    apt-get -y update
    apt-get install -y --force-yes libhdfs3 libhdfs3-dev

    mkdir temp && cd temp && git clone https://github.com/zeromq/cppzmq \
    && cd cppzmq && git reset --hard 4648ebc9643119cff2a433dff4609f1a5cb640ec \
    && cp zmq.hpp /usr/local/include
  SHELL
end
