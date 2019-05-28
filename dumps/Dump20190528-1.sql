CREATE DATABASE  IF NOT EXISTS `escada` /*!40100 DEFAULT CHARACTER SET latin1 */;
USE `escada`;
-- MySQL dump 10.13  Distrib 5.6.17, for osx10.6 (i386)
--
-- Host: localhost    Database: escada
-- ------------------------------------------------------
-- Server version	8.0.13

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `config`
--

DROP TABLE IF EXISTS `config`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `config` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `device` varchar(45) NOT NULL,
  `adr` varchar(30) DEFAULT NULL,
  `ip` varchar(30) NOT NULL DEFAULT '',
  `object` varchar(45) DEFAULT NULL,
  `regim` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `q_request` tinyint(3) unsigned NOT NULL,
  `deep` int(10) DEFAULT '0',
  `tmdt` int(10) unsigned DEFAULT '0',
  `int_light` int(10) unsigned NOT NULL DEFAULT '0',
  `int_electro` int(10) unsigned NOT NULL DEFAULT '0',
  `last_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `chng` tinyint(1) NOT NULL DEFAULT '0',
  `log` int(10) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `config`
--

LOCK TABLES `config` WRITE;
/*!40000 ALTER TABLE `config` DISABLE KEYS */;
INSERT INTO `config` VALUES (1,'34013185','0','172.16.0.230','101',3,5,0,10,3600,7,'2017-03-02 19:35:06',0,3);
/*!40000 ALTER TABLE `config` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `data`
--

DROP TABLE IF EXISTS `data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `data` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `type` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `value` double NOT NULL DEFAULT '0',
  `sensor_channel` int(11) NOT NULL DEFAULT '0',
  `status` smallint(5) unsigned NOT NULL DEFAULT '0',
  `measure_type` varchar(45) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=1762379 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `data`
--

LOCK TABLES `data` WRITE;
/*!40000 ALTER TABLE `data` DISABLE KEYS */;
/*!40000 ALTER TABLE `data` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `device`
--

DROP TABLE IF EXISTS `device`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `device` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `uuid` varchar(45) NOT NULL DEFAULT '0',
  `thread` int(10) unsigned NOT NULL DEFAULT '0',
  `adr` int(10) NOT NULL DEFAULT '0',
  `number` varchar(30) CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL DEFAULT '',
  `object` varchar(45) NOT NULL DEFAULT '0',
  `akt` tinyint(1) NOT NULL DEFAULT '1',
  `lastdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `qatt` int(10) unsigned NOT NULL DEFAULT '0',
  `qerrors` int(10) unsigned NOT NULL DEFAULT '0',
  `conn` tinyint(1) NOT NULL DEFAULT '0',
  `devtim` datetime NOT NULL,
  `chng` tinyint(1) NOT NULL DEFAULT '0',
  `req` tinyint(1) NOT NULL DEFAULT '0',
  `measure_type` varchar(45) NOT NULL DEFAULT '1',
  `name` varchar(50) NOT NULL,
  `port` varchar(45) DEFAULT NULL,
  `protocol` int(11) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `device`
--

LOCK TABLES `device` WRITE;
/*!40000 ALTER TABLE `device` DISABLE KEYS */;
INSERT INTO `device` VALUES (1,'E38C561F-9E88-407E-A465-83803A625627',1,1,'','041DED21-D211-4C0B-BCD6-02E392654332',1,'2019-05-25 09:47:03',0,0,1,'2019-01-01 00:00:00',0,1,'33','Счетчик Меркурий','/dev/ttyS0',7),(2,'29A52371-E9EC-4D1F-8BCB-80F489A96DD3',1,2,'','041DED21-D211-4C0B-BCD6-02E392654332',1,'2019-05-25 09:47:03',0,0,1,'2019-01-01 00:00:00',0,1,'33','Счетчик СЕ','/dev/ttyS0',7);
/*!40000 ALTER TABLE `device` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `device_type`
--

DROP TABLE IF EXISTS `device_type`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `device_type` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `title` varchar(45) NOT NULL,
  `lib` varchar(45) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `device_type`
--

LOCK TABLES `device_type` WRITE;
/*!40000 ALTER TABLE `device_type` DISABLE KEYS */;
INSERT INTO `device_type` VALUES (1,'Меркурий-230','mercury230.o'),(2,'Энергомера СЕ','mercury230.o'),(3,'СЕТ-4ТМ','mercury230.o'),(4,'Умный светильник','zigbee.o');
/*!40000 ALTER TABLE `device_type` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `edizm`
--

DROP TABLE IF EXISTS `edizm`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `edizm` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(30) CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL DEFAULT '',
  `knt` double NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=16 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `edizm`
--

LOCK TABLES `edizm` WRITE;
/*!40000 ALTER TABLE `edizm` DISABLE KEYS */;
INSERT INTO `edizm` VALUES (1,'t,C',1),(2,'P,MPa',1000000),(3,'P,kPa',1000),(4,'P,Pa',1),(5,'Q,m3/h',1),(6,'V,m3',1),(7,'W,kW',1000),(8,'Q,kJ',1000),(9,'Q,J',1),(10,'Q,GJ',1000000),(11,'Q,Gkal',1000000),(12,'H, kG/kg',1),(13,'dB',1),(14,'M,kg',1),(15,'G,t/h',1);
/*!40000 ALTER TABLE `edizm` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `errors`
--

DROP TABLE IF EXISTS `errors`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `errors` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `code` int(10) unsigned NOT NULL DEFAULT '0',
  `descr` text CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `errors`
--

LOCK TABLES `errors` WRITE;
/*!40000 ALTER TABLE `errors` DISABLE KEYS */;
INSERT INTO `errors` VALUES (1,385875969,'controller started');
/*!40000 ALTER TABLE `errors` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `info`
--

DROP TABLE IF EXISTS `info`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `info` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `log` varchar(100) NOT NULL,
  `time` varchar(50) NOT NULL,
  `linux` varchar(100) NOT NULL,
  `hardware` varchar(100) NOT NULL,
  `base_name` varchar(45) NOT NULL,
  `software` varchar(45) NOT NULL,
  `ip` varchar(45) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `info`
--

LOCK TABLES `info` WRITE;
/*!40000 ALTER TABLE `info` DISABLE KEYS */;
INSERT INTO `info` VALUES (1,'2017-06-02 07:36:24','log/kernel-20170602_1218.log','2013-07-05 12-32-11','Linux 3.16.0-4-586','i586','escada','0.218.017','1.1.1.1');
/*!40000 ALTER TABLE `info` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `interfaces`
--

DROP TABLE IF EXISTS `interfaces`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `interfaces` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(45) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=6 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `interfaces`
--

LOCK TABLES `interfaces` WRITE;
/*!40000 ALTER TABLE `interfaces` DISABLE KEYS */;
INSERT INTO `interfaces` VALUES (1,'RS-232'),(2,'RS-485'),(3,'Wireless'),(4,'Ethernet'),(5,'CAN');
/*!40000 ALTER TABLE `interfaces` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `light_answer`
--

DROP TABLE IF EXISTS `light_answer`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `light_answer` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `addr` text NOT NULL,
  `data` text NOT NULL,
  `dateIn` timestamp NOT NULL,
  `dateOut` timestamp NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `light_answer`
--

LOCK TABLES `light_answer` WRITE;
/*!40000 ALTER TABLE `light_answer` DISABLE KEYS */;
/*!40000 ALTER TABLE `light_answer` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `light_message`
--

DROP TABLE IF EXISTS `light_message`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `light_message` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `addr` text NOT NULL,
  `data` text NOT NULL,
  `dateIn` timestamp NOT NULL,
  `deteOut` timestamp NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=744721 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `light_message`
--

LOCK TABLES `light_message` WRITE;
/*!40000 ALTER TABLE `light_message` DISABLE KEYS */;
INSERT INTO `light_message` VALUES (744658,'š•B','t1=74.55, t2=inf, dt=0.00, h1=312.42, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-05 11:38:03','0000-00-00 00:00:00'),(744659,'','t1=inf, t2=46.37, dt=0.00, h1=0.00, h2=194.58, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-05 11:38:04','0000-00-00 00:00:00'),(744660,'','t1=inf, t2=14.76, dt=0.00, h1=0.00, h2=62.48, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-05 11:38:05','0000-00-00 00:00:00'),(744661,'•B','t1=74.53, t2=inf, dt=0.00, h1=312.34, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-05 11:42:47','0000-00-00 00:00:00'),(744662,'','t1=inf, t2=46.48, dt=0.00, h1=0.00, h2=195.04, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-05 11:42:49','0000-00-00 00:00:00'),(744663,'','t1=inf, t2=14.76, dt=0.00, h1=0.00, h2=62.48, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-05 11:42:50','0000-00-00 00:00:00'),(744664,'R¸ˆA','t1=17.09, t2=inf, dt=0.00, h1=72.23, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-05 11:42:53','0000-00-00 00:00:00'),(744665,'Ãõ•B','t1=74.98, t2=inf, dt=0.00, h1=314.22, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:06:04','0000-00-00 00:00:00'),(744666,'–B','t1=75.01, t2=inf, dt=0.00, h1=314.35, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:07:54','0000-00-00 00:00:00'),(744667,'…ë•B','t1=74.96, t2=inf, dt=0.00, h1=314.14, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:08:25','0000-00-00 00:00:00'),(744668,'š™ŽB','t1=71.30, t2=inf, dt=0.00, h1=298.81, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:45:54','0000-00-00 00:00:00'),(744669,'{”ŽB','t1=71.29, t2=inf, dt=0.00, h1=298.77, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:46:18','0000-00-00 00:00:00'),(744670,'','t1=inf, t2=47.56, dt=0.00, h1=0.00, h2=199.55, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:46:20','0000-00-00 00:00:00'),(744671,'','t1=inf, t2=15.12, dt=0.00, h1=0.00, h2=63.99, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:46:21','0000-00-00 00:00:00'),(744672,'…ë—A','t1=18.99, t2=inf, dt=0.00, h1=80.18, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:46:24','0000-00-00 00:00:00'),(744673,'×£ŽB','t1=71.32, t2=inf, dt=0.00, h1=298.90, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:47:17','0000-00-00 00:00:00'),(744674,'','t1=inf, t2=47.71, dt=0.00, h1=0.00, h2=200.17, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:47:18','0000-00-00 00:00:00'),(744675,'','t1=inf, t2=15.13, dt=0.00, h1=0.00, h2=64.03, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:47:20','0000-00-00 00:00:00'),(744676,'ìQ˜A','t1=19.04, t2=inf, dt=0.00, h1=80.39, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:47:23','0000-00-00 00:00:00'),(744677,'{”ŽB','t1=71.29, t2=inf, dt=0.00, h1=298.77, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:51:54','0000-00-00 00:00:00'),(744678,'','t1=inf, t2=47.93, dt=0.00, h1=0.00, h2=201.09, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:51:56','0000-00-00 00:00:00'),(744679,'','t1=inf, t2=15.13, dt=0.00, h1=0.00, h2=64.03, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:51:57','0000-00-00 00:00:00'),(744680,'=ŠŽB','t1=71.27, t2=inf, dt=0.00, h1=298.69, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:52:36','0000-00-00 00:00:00'),(744681,'','t1=inf, t2=47.94, dt=0.00, h1=0.00, h2=201.14, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:52:38','0000-00-00 00:00:00'),(744682,'','t1=inf, t2=15.14, dt=0.00, h1=0.00, h2=64.07, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:52:39','0000-00-00 00:00:00'),(744683,'HášA','t1=19.36, t2=inf, dt=0.00, h1=81.73, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:52:42','0000-00-00 00:00:00'),(744684,'{”ŽB','t1=71.29, t2=inf, dt=0.00, h1=298.77, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:53:30','0000-00-00 00:00:00'),(744685,'','t1=inf, t2=47.92, dt=0.00, h1=0.00, h2=201.05, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:53:32','0000-00-00 00:00:00'),(744686,'','t1=inf, t2=15.16, dt=0.00, h1=0.00, h2=64.15, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:53:33','0000-00-00 00:00:00'),(744687,'33›A','t1=19.40, t2=inf, dt=0.00, h1=81.90, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:53:35','0000-00-00 00:00:00'),(744688,'…ŽB','t1=71.26, t2=inf, dt=0.00, h1=298.64, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:53:54','0000-00-00 00:00:00'),(744689,'','t1=inf, t2=47.96, dt=0.00, h1=0.00, h2=201.22, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:53:56','0000-00-00 00:00:00'),(744690,'','t1=inf, t2=15.15, dt=0.00, h1=0.00, h2=64.11, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:53:57','0000-00-00 00:00:00'),(744691,'¤p›A','t1=19.43, t2=inf, dt=0.00, h1=82.02, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:54:00','0000-00-00 00:00:00'),(744692,'¸žŽB','t1=71.31, t2=inf, dt=0.00, h1=298.85, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:58:16','0000-00-00 00:00:00'),(744693,'','t1=inf, t2=48.00, dt=0.00, h1=0.00, h2=201.39, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 06:58:17','0000-00-00 00:00:00'),(744694,'','t1=inf, t2=15.20, dt=0.00, h1=0.00, h2=64.32, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:58:19','0000-00-00 00:00:00'),(744695,'=\nA','t1=19.63, t2=inf, dt=0.00, h1=82.86, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 06:58:21','0000-00-00 00:00:00'),(744696,'{”ŽB','t1=71.29, t2=inf, dt=0.00, h1=298.77, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 07:01:49','0000-00-00 00:00:00'),(744697,'','t1=inf, t2=47.99, dt=0.00, h1=0.00, h2=201.34, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 07:01:50','0000-00-00 00:00:00'),(744698,'','t1=inf, t2=15.18, dt=0.00, h1=0.00, h2=64.24, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 07:01:52','0000-00-00 00:00:00'),(744699,'\n×A','t1=19.73, t2=inf, dt=0.00, h1=83.28, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 07:01:54','0000-00-00 00:00:00'),(744700,'áúŽB','t1=71.49, t2=inf, dt=0.00, h1=299.61, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:40:37','0000-00-00 00:00:00'),(744701,'','t1=inf, t2=47.81, dt=0.00, h1=0.00, h2=200.59, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:40:38','0000-00-00 00:00:00'),(744702,'','t1=inf, t2=16.04, dt=0.00, h1=0.00, h2=67.84, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:40:40','0000-00-00 00:00:00'),(744703,'ff¬A','t1=21.55, t2=inf, dt=0.00, h1=90.89, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:40:43','0000-00-00 00:00:00'),(744704,'','t1=71.50, t2=inf, dt=0.00, h1=299.65, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:41:07','0000-00-00 00:00:00'),(744705,'','t1=inf, t2=47.81, dt=0.00, h1=0.00, h2=200.59, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:41:09','0000-00-00 00:00:00'),(744706,'','t1=inf, t2=16.02, dt=0.00, h1=0.00, h2=67.75, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:41:10','0000-00-00 00:00:00'),(744707,'×£¬A','t1=21.58, t2=inf, dt=0.00, h1=91.01, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:41:13','0000-00-00 00:00:00'),(744708,'ÃõŽB','t1=71.48, t2=inf, dt=0.00, h1=299.57, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:42:47','0000-00-00 00:00:00'),(744709,'','t1=inf, t2=47.81, dt=0.00, h1=0.00, h2=200.59, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:42:49','0000-00-00 00:00:00'),(744710,'','t1=inf, t2=16.07, dt=0.00, h1=0.00, h2=67.96, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:42:50','0000-00-00 00:00:00'),(744711,', dh=0.00, vl=-nan, m1=0.000, v2=0.000, m2=0.000, q1=397291839316328920264278758190481408.0000','t1=74934263501039696138113246438621184.00, t2=34473205768963227648.00, dt=0.00, h1=34466065540452450','2013-10-06 08:43:17','0000-00-00 00:00:00'),(744712,'00, q1=-0.0000','t1=-0.00, t2=148061291365975594574762999808.00, dt=0.00, h1=148030624318035839098745257984.00, h2=-0','2013-10-06 08:49:06','0000-00-00 00:00:00'),(744713,'fæŽB','t1=71.45, t2=inf, dt=0.00, h1=299.44, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:51:23','0000-00-00 00:00:00'),(744714,'','t1=inf, t2=47.81, dt=0.00, h1=0.00, h2=200.59, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:51:24','0000-00-00 00:00:00'),(744715,'','t1=inf, t2=16.26, dt=0.00, h1=0.00, h2=68.76, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:51:26','0000-00-00 00:00:00'),(744716,'®¯A','t1=21.96, t2=inf, dt=0.00, h1=92.60, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:51:29','0000-00-00 00:00:00'),(744717,'¤ðŽB','t1=71.47, t2=inf, dt=0.00, h1=299.52, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:51:41','0000-00-00 00:00:00'),(744718,'','t1=inf, t2=47.84, dt=0.00, h1=0.00, h2=200.72, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0','2013-10-06 08:51:43','0000-00-00 00:00:00'),(744719,'','t1=inf, t2=16.26, dt=0.00, h1=0.00, h2=68.76, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:51:44','0000-00-00 00:00:00'),(744720,'\n×¯A','t1=21.98, t2=inf, dt=0.00, h1=92.69, h2=0.00, dh=0.00, vl=0.000, m1=0.000, v2=0.000, m2=0.000, q1=0.','2013-10-06 08:51:47','0000-00-00 00:00:00');
/*!40000 ALTER TABLE `light_message` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `measure_type`
--

DROP TABLE IF EXISTS `measure_type`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `measure_type` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `title` varchar(100) NOT NULL DEFAULT '',
  `createdAt` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updatedAt` timestamp NULL DEFAULT NULL,
  `c_value` varchar(10) DEFAULT NULL,
  `uuid` varchar(45) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=41 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `measure_type`
--

LOCK TABLES `measure_type` WRITE;
/*!40000 ALTER TABLE `measure_type` DISABLE KEYS */;
INSERT INTO `measure_type` VALUES (1,'Температура подающего',NULL,NULL,NULL,''),(2,'Температура обратного',NULL,NULL,NULL,''),(3,'Температура на входе в СО',NULL,NULL,NULL,''),(4,'Температура на выходе из СО',NULL,NULL,NULL,''),(5,'Температура наружного воздуха',NULL,NULL,NULL,''),(6,'Давление теплоносителя на входе',NULL,NULL,NULL,''),(7,'Давление теплоносителя на выходе',NULL,NULL,NULL,''),(8,'Давление газа на входе',NULL,NULL,NULL,''),(9,'Суммарный интеграл энтальпии',NULL,NULL,NULL,''),(10,'Суммарный интеграл энтальпии',NULL,NULL,NULL,''),(11,'Уровень затуханмя сигнала',NULL,NULL,NULL,''),(14,'Температура ГВС',NULL,NULL,NULL,''),(15,'Температура ХВС',NULL,NULL,NULL,''),(16,'Интегральное значение удельной энтальпии подающей',NULL,NULL,NULL,''),(17,'Интегральное значение удельной энтальпии обратной',NULL,NULL,NULL,''),(18,'Интегральное значение удельной энтальпии',NULL,NULL,NULL,''),(19,'Интегральное значение удельной энтальпии',NULL,NULL,NULL,''),(20,'Интегральное значение удельной энтальпии',NULL,NULL,NULL,''),(21,'Объемный расход подающий',NULL,NULL,NULL,''),(22,'Объемный расход обратный',NULL,NULL,NULL,''),(23,'Объемный расход ХВС',NULL,NULL,NULL,''),(24,'Массовый расход подающий',NULL,NULL,NULL,''),(25,'Массовый расход обратный',NULL,NULL,NULL,''),(26,'Массовый расход утечек',NULL,NULL,NULL,''),(27,'Массовый расход на СО',NULL,NULL,NULL,''),(28,'Массовый расход ГВС',NULL,NULL,NULL,''),(29,'Массовый расход ХВС',NULL,NULL,NULL,''),(30,'Тепловая энергия на выходе',NULL,NULL,NULL,''),(31,'Тепловая энергия потребленная',NULL,NULL,NULL,''),(32,'Тепловая энергия в системе СО',NULL,NULL,NULL,''),(33,'Мощность электрической энергии',NULL,NULL,NULL,''),(34,'Потребленная мощность',NULL,NULL,NULL,''),(35,'Давление в системе ГВС',NULL,NULL,NULL,''),(36,'Давление в системе ХВС',NULL,NULL,NULL,''),(37,'Тепловая энергия на входе',NULL,NULL,NULL,''),(38,'Объемный расход ГВС',NULL,NULL,NULL,''),(39,'Напряжение',NULL,NULL,NULL,''),(40,'Потребленная энергия накопительная',NULL,NULL,NULL,'');
/*!40000 ALTER TABLE `measure_type` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `protocols`
--

DROP TABLE IF EXISTS `protocols`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `protocols` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(50) NOT NULL DEFAULT '',
  `type` int(10) unsigned NOT NULL DEFAULT '0',
  `protocol` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=18 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `protocols`
--

LOCK TABLES `protocols` WRITE;
/*!40000 ALTER TABLE `protocols` DISABLE KEYS */;
INSERT INTO `protocols` VALUES (1,'not defined',0,0),(2,'wireless v.1',1,1),(3,'FT 1.2',1,2),(4,'Modbus',2,13),(5,'протокол MOSCAD',2,15),(6,'протокол СПГ*41',3,16),(7,'протокол СЭТ',3,14),(8,'own 485',4,3),(9,'own 485',5,4),(10,'LC-HC v.1',6,5),(11,'own LC-HC (stack)',6,6),(12,'CRQ based',7,7),(13,'xml type',7,8),(14,'CAN-BUS',11,9),(15,'FT 1.1',11,10),(16,'wireless v.2',11,11),(17,'SPBUS',12,12);
/*!40000 ALTER TABLE `protocols` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `register`
--

DROP TABLE IF EXISTS `register`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `register` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `code` int(10) unsigned NOT NULL DEFAULT '0',
  `device` varchar(45) NOT NULL,
  `date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=259836 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `register`
--

LOCK TABLES `register` WRITE;
/*!40000 ALTER TABLE `register` DISABLE KEYS */;
/*!40000 ALTER TABLE `register` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sensor_channel`
--

DROP TABLE IF EXISTS `sensor_channel`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sensor_channel` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `title` varchar(150) NOT NULL DEFAULT '',
  `device` varchar(45) NOT NULL,
  `measureType` varchar(45) NOT NULL,
  `register` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sensor_channel`
--

LOCK TABLES `sensor_channel` WRITE;
/*!40000 ALTER TABLE `sensor_channel` DISABLE KEYS */;
INSERT INTO `sensor_channel` VALUES (1,'Мощность электрической энергии','1','33',0),(2,'Потребленная энергия накопительная','1','40',1),(3,'Мощность электрической энергии','2','33',0),(4,'Потребленная энергия накопительная','2','40',1);
/*!40000 ALTER TABLE `sensor_channel` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sensor_config`
--

DROP TABLE IF EXISTS `sensor_config`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sensor_config` (
  `id` int(11) NOT NULL,
  `uuid` varchar(45) NOT NULL,
  `device` varchar(45) NOT NULL,
  `mac` varchar(45) NOT NULL,
  `object` varchar(45) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sensor_config`
--

LOCK TABLES `sensor_config` WRITE;
/*!40000 ALTER TABLE `sensor_config` DISABLE KEYS */;
/*!40000 ALTER TABLE `sensor_config` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sensors`
--

DROP TABLE IF EXISTS `sensors`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sensors` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `uuid` varchar(45) NOT NULL DEFAULT '0',
  `address` int(10) unsigned NOT NULL DEFAULT '0',
  `module` int(10) NOT NULL,
  `device` int(10) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=24 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sensors`
--

LOCK TABLES `sensors` WRITE;
/*!40000 ALTER TABLE `sensors` DISABLE KEYS */;
/*!40000 ALTER TABLE `sensors` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `source`
--

DROP TABLE IF EXISTS `source`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `source` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(30) CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `source`
--

LOCK TABLES `source` WRITE;
/*!40000 ALTER TABLE `source` DISABLE KEYS */;
/*!40000 ALTER TABLE `source` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `stat`
--

DROP TABLE IF EXISTS `stat`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `stat` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `qreboot` int(10) unsigned NOT NULL DEFAULT '0',
  `timeinit` int(10) unsigned NOT NULL DEFAULT '0',
  `timezapr` int(10) unsigned NOT NULL DEFAULT '0',
  `timeans` int(10) unsigned NOT NULL DEFAULT '0',
  `type` int(10) unsigned NOT NULL,
  `cpu` double NOT NULL,
  `mem` double NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=290421 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `stat`
--

LOCK TABLES `stat` WRITE;
/*!40000 ALTER TABLE `stat` DISABLE KEYS */;
/*!40000 ALTER TABLE `stat` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `threads`
--

DROP TABLE IF EXISTS `threads`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `threads` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `port` varchar(45) NOT NULL,
  `speed` int(10) unsigned NOT NULL,
  `title` varchar(45) NOT NULL,
  `status` int(1) unsigned NOT NULL,
  `work` int(1) unsigned NOT NULL,
  `deviceType` varchar(50) NOT NULL,
  `ctime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `message` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=cp1251;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `threads`
--

LOCK TABLES `threads` WRITE;
/*!40000 ALTER TABLE `threads` DISABLE KEYS */;
INSERT INTO `threads` VALUES (1,'/ttys0',19200,'Счетчики МЭК',1,1,'1','2019-05-25 11:52:21','read increments');
/*!40000 ALTER TABLE `threads` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2019-05-28 15:13:36
