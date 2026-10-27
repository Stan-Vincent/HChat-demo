-- MySQL dump 10.13  Distrib 8.0.44, for Win64 (x86_64)
--
-- Host: 127.0.0.1    Database: Hcat
-- ------------------------------------------------------
-- Server version	8.0.44

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `chat_message`
--

DROP TABLE IF EXISTS `chat_message`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `chat_message` (
  `message_id` int NOT NULL AUTO_INCREMENT,
  `thread_id` int NOT NULL,
  `sender_id` int NOT NULL,
  `recv_id` int NOT NULL,
  `content` varchar(2048) NOT NULL,
  `msg_type` int NOT NULL DEFAULT '0',
  `unique_name` varchar(128) DEFAULT NULL,
  `total_size` bigint NOT NULL DEFAULT '0',
  `md5` varchar(40) DEFAULT NULL,
  `created_at` datetime NOT NULL,
  `updated_at` datetime NOT NULL,
  `status` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`message_id`),
  KEY `idx_thread_msg` (`thread_id`,`message_id`)
) ENGINE=InnoDB AUTO_INCREMENT=290 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `chat_message`
--

LOCK TABLES `chat_message` WRITE;
/*!40000 ALTER TABLE `chat_message` DISABLE KEYS */;
INSERT INTO `chat_message` VALUES (1,1,2,3,'你好呀，这是第 1-1 条测试消息',0,NULL,0,NULL,'2026-10-10 07:46:43','2026-10-10 07:46:43',0),(2,1,3,2,'收到！消息 1-2 已送达',0,NULL,0,NULL,'2026-10-10 07:49:43','2026-10-10 07:49:43',2),(3,1,2,3,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 1-3',0,NULL,0,NULL,'2026-10-10 07:52:43','2026-10-10 07:52:43',2),(4,1,3,2,'好的，收到 [msg-1-4]',0,NULL,0,NULL,'2026-10-10 07:55:43','2026-10-10 07:55:43',2),(5,2,2,5,'你好呀，这是第 2-1 条测试消息',0,NULL,0,NULL,'2026-10-10 08:16:43','2026-10-10 08:16:43',0),(6,2,5,2,'收到！消息 2-2 已送达',0,NULL,0,NULL,'2026-10-10 08:19:43','2026-10-10 08:19:43',2),(7,2,2,5,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 2-3',0,NULL,0,NULL,'2026-10-10 08:22:43','2026-10-10 08:22:43',2),(8,2,5,2,'好的，收到 [msg-2-4]',0,NULL,0,NULL,'2026-10-10 08:25:43','2026-10-10 08:25:43',2),(9,3,2,6,'你好呀，这是第 3-1 条测试消息',0,NULL,0,NULL,'2026-10-10 08:46:43','2026-10-10 08:46:43',0),(10,3,6,2,'收到！消息 3-2 已送达',0,NULL,0,NULL,'2026-10-10 08:49:43','2026-10-10 08:49:43',2),(11,3,2,6,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 3-3',0,NULL,0,NULL,'2026-10-10 08:52:43','2026-10-10 08:52:43',2),(12,3,6,2,'好的，收到 [msg-3-4]',0,NULL,0,NULL,'2026-10-10 08:55:43','2026-10-10 08:55:43',2),(13,4,2,7,'你好呀，这是第 4-1 条测试消息',0,NULL,0,NULL,'2026-10-10 09:16:43','2026-10-10 09:16:43',0),(14,4,7,2,'收到！消息 4-2 已送达',0,NULL,0,NULL,'2026-10-10 09:19:43','2026-10-10 09:19:43',2),(15,4,2,7,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 4-3',0,NULL,0,NULL,'2026-10-10 09:22:43','2026-10-10 09:22:43',2),(16,4,7,2,'好的，收到 [msg-4-4]',0,NULL,0,NULL,'2026-10-10 09:25:43','2026-10-10 09:25:43',2),(17,5,2,8,'你好呀，这是第 5-1 条测试消息',0,NULL,0,NULL,'2026-10-10 09:46:43','2026-10-10 09:46:43',0),(18,5,8,2,'收到！消息 5-2 已送达',0,NULL,0,NULL,'2026-10-10 09:49:43','2026-10-10 09:49:43',2),(19,5,2,8,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 5-3',0,NULL,0,NULL,'2026-10-10 09:52:43','2026-10-10 09:52:43',2),(20,5,8,2,'好的，收到 [msg-5-4]',0,NULL,0,NULL,'2026-10-10 09:55:43','2026-10-10 09:55:43',2),(21,6,3,5,'你好呀，这是第 6-1 条测试消息',0,NULL,0,NULL,'2026-10-10 10:16:43','2026-10-10 10:16:43',0),(22,6,5,3,'收到！消息 6-2 已送达',0,NULL,0,NULL,'2026-10-10 10:19:43','2026-10-10 10:19:43',2),(23,6,3,5,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 6-3',0,NULL,0,NULL,'2026-10-10 10:22:43','2026-10-10 10:22:43',2),(24,6,5,3,'好的，收到 [msg-6-4]',0,NULL,0,NULL,'2026-10-10 10:25:43','2026-10-10 10:25:43',2),(25,7,3,6,'你好呀，这是第 7-1 条测试消息',0,NULL,0,NULL,'2026-10-10 10:46:43','2026-10-10 10:46:43',0),(26,7,6,3,'收到！消息 7-2 已送达',0,NULL,0,NULL,'2026-10-10 10:49:43','2026-10-10 10:49:43',2),(27,7,3,6,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 7-3',0,NULL,0,NULL,'2026-10-10 10:52:43','2026-10-10 10:52:43',2),(28,7,6,3,'好的，收到 [msg-7-4]',0,NULL,0,NULL,'2026-10-10 10:55:43','2026-10-10 10:55:43',2),(29,8,3,7,'你好呀，这是第 8-1 条测试消息',0,NULL,0,NULL,'2026-10-10 11:16:43','2026-10-10 11:16:43',0),(30,8,7,3,'收到！消息 8-2 已送达',0,NULL,0,NULL,'2026-10-10 11:19:43','2026-10-10 11:19:43',2),(31,8,3,7,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 8-3',0,NULL,0,NULL,'2026-10-10 11:22:43','2026-10-10 11:22:43',2),(32,8,7,3,'好的，收到 [msg-8-4]',0,NULL,0,NULL,'2026-10-10 11:25:43','2026-10-10 11:25:43',2),(33,9,3,8,'你好呀，这是第 9-1 条测试消息',0,NULL,0,NULL,'2026-10-10 11:46:43','2026-10-10 11:46:43',0),(34,9,8,3,'收到！消息 9-2 已送达',0,NULL,0,NULL,'2026-10-10 11:49:43','2026-10-10 11:49:43',2),(35,9,3,8,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 9-3',0,NULL,0,NULL,'2026-10-10 11:52:43','2026-10-10 11:52:43',2),(36,9,8,3,'好的，收到 [msg-9-4]',0,NULL,0,NULL,'2026-10-10 11:55:43','2026-10-10 11:55:43',2),(37,10,5,6,'你好呀，这是第 10-1 条测试消息',0,NULL,0,NULL,'2026-10-10 12:16:43','2026-10-10 12:16:43',0),(38,10,6,5,'收到！消息 10-2 已送达',0,NULL,0,NULL,'2026-10-10 12:19:43','2026-10-10 12:19:43',2),(39,10,5,6,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 10-3',0,NULL,0,NULL,'2026-10-10 12:22:43','2026-10-10 12:22:43',2),(40,10,6,5,'好的，收到 [msg-10-4]',0,NULL,0,NULL,'2026-10-10 12:25:43','2026-10-10 12:25:43',2),(41,11,5,7,'你好呀，这是第 11-1 条测试消息',0,NULL,0,NULL,'2026-10-10 12:46:43','2026-10-10 12:46:43',0),(42,11,7,5,'收到！消息 11-2 已送达',0,NULL,0,NULL,'2026-10-10 12:49:43','2026-10-10 12:49:43',2),(43,11,5,7,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 11-3',0,NULL,0,NULL,'2026-10-10 12:52:43','2026-10-10 12:52:43',2),(44,11,7,5,'好的，收到 [msg-11-4]',0,NULL,0,NULL,'2026-10-10 12:55:43','2026-10-10 12:55:43',2),(45,12,5,8,'你好呀，这是第 12-1 条测试消息',0,NULL,0,NULL,'2026-10-10 13:16:43','2026-10-10 13:16:43',0),(46,12,8,5,'收到！消息 12-2 已送达',0,NULL,0,NULL,'2026-10-10 13:19:43','2026-10-10 13:19:43',2),(47,12,5,8,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 12-3',0,NULL,0,NULL,'2026-10-10 13:22:43','2026-10-10 13:22:43',2),(48,12,8,5,'好的，收到 [msg-12-4]',0,NULL,0,NULL,'2026-10-10 13:25:43','2026-10-10 13:25:43',2),(49,13,6,7,'你好呀，这是第 13-1 条测试消息',0,NULL,0,NULL,'2026-10-10 13:46:43','2026-10-10 13:46:43',0),(50,13,7,6,'收到！消息 13-2 已送达',0,NULL,0,NULL,'2026-10-10 13:49:43','2026-10-10 13:49:43',2),(51,13,6,7,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 13-3',0,NULL,0,NULL,'2026-10-10 13:52:43','2026-10-10 13:52:43',2),(52,13,7,6,'好的，收到 [msg-13-4]',0,NULL,0,NULL,'2026-10-10 13:55:43','2026-10-10 13:55:43',2),(53,14,6,8,'你好呀，这是第 14-1 条测试消息',0,NULL,0,NULL,'2026-10-10 14:16:43','2026-10-10 14:16:43',0),(54,14,8,6,'收到！消息 14-2 已送达',0,NULL,0,NULL,'2026-10-10 14:19:43','2026-10-10 14:19:43',2),(55,14,6,8,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 14-3',0,NULL,0,NULL,'2026-10-10 14:22:43','2026-10-10 14:22:43',2),(56,14,8,6,'好的，收到 [msg-14-4]',0,NULL,0,NULL,'2026-10-10 14:25:43','2026-10-10 14:25:43',2),(57,15,7,8,'你好呀，这是第 15-1 条测试消息',0,NULL,0,NULL,'2026-10-10 14:46:43','2026-10-10 14:46:43',0),(58,15,8,7,'收到！消息 15-2 已送达',0,NULL,0,NULL,'2026-10-10 14:49:43','2026-10-10 14:49:43',2),(59,15,7,8,'这是一条比较长的消息，用来测试气泡自动换行的效果。如果气泡宽度超过屏幕它应该会自动折行，而不是把窗口撑开或者被裁掉，这段文字故意写长一点，方便观察渲染效果。 15-3',0,NULL,0,NULL,'2026-10-10 14:52:43','2026-10-10 14:52:43',2),(60,15,8,7,'好的，收到 [msg-15-4]',0,NULL,0,NULL,'2026-10-10 14:55:43','2026-10-10 14:55:43',2),(61,1,2,3,'我是 testuser，交个朋友吧~',0,NULL,0,NULL,'2026-10-10 15:33:08','2026-10-10 15:33:08',2),(62,1,3,2,'We are friends now!',0,NULL,0,NULL,'2026-10-10 15:33:08','2026-10-10 15:33:08',2),(170,1,3,2,'你好',0,'{04458867-4933-422c-9cc1-6861fba39321}',0,'','2026-10-11 12:20:18','2026-10-11 12:20:18',2),(171,6,3,5,'？',0,'{81782246-6483-463f-b4d5-8ff36b503be2}',0,'','2026-10-11 12:20:31','2026-10-11 12:20:31',2),(172,1,3,2,'?',0,'{d9095485-3375-4433-b3fa-fa8bc1bf46d5}',0,'','2026-10-11 12:20:49','2026-10-11 12:20:49',2),(173,1,3,2,'hi',0,'{b6bc38f5-efe1-45f4-bd83-b2b5e7cc20be}',0,'','2026-10-11 12:39:55','2026-10-11 12:39:55',2),(174,1,3,2,'?',0,'{0ccaf684-5796-4236-813f-156b4dd67168}',0,'','2026-10-11 12:40:28','2026-10-11 12:40:28',2),(175,6,5,3,'hi',0,'{4765ff83-e843-4401-848a-08bd0c18c52c}',0,'','2026-10-11 15:25:01','2026-10-11 15:25:01',2),(176,6,3,5,'upload/1791704343050_6ae4f1cdb9616459.jpg',1,'{9d02a64c-b2dd-462d-bba5-838748f8a410}',0,'','2026-10-11 15:39:03','2026-10-11 15:39:03',2),(177,6,3,5,'upload/1791704359164_ee38c448e752aeb7.png',1,'{3c3c26f8-a9b4-43a7-96dc-08fb1bba4ebc}',0,'','2026-10-11 15:39:19','2026-10-11 15:39:19',2),(178,6,3,5,'111',0,'{89c3704e-8eaf-40cb-b085-7058318c731c}',0,'','2026-10-11 20:30:27','2026-10-11 20:30:27',2),(179,2,5,2,'hi',0,'{d8be4cba-78e0-4ab0-a844-a2eea2fe0bd7}',0,'','2026-10-12 20:44:58','2026-10-12 20:44:58',2),(180,2,5,2,'?',0,'{89df79c8-2591-48c1-935e-25b173426a9c}',0,'','2026-10-12 22:49:47','2026-10-12 22:49:47',2),(181,2,5,2,'?',0,'{47d3ae3d-6975-4df4-a4f2-2982f4920eb0}',0,'','2026-10-13 17:37:38','2026-10-13 17:37:38',2),(182,2,5,2,'upload/1791946377407_601a79fe774cff23.jpg',1,'{d7c368df-28fa-43b6-871b-61b41b37883b}',0,'','2026-10-14 10:52:57','2026-10-14 10:52:57',2),(183,2,5,2,'print(List1)\n',0,'{0e7b5f6a-c8cc-4ee6-bd88-6fef7bec1775}',0,'','2026-10-14 10:59:48','2026-10-14 10:59:48',2),(184,2,5,2,'upload/1791946825914_c14f8b092ac5902b.jpg',1,'{b2dc16b6-47de-44ad-95f7-56bf3b88b5da}',0,'','2026-10-14 11:00:25','2026-10-14 11:00:25',2),(185,2,5,2,'  ',0,'{b9b08d55-dc71-426a-a9f1-50e4fc7bf6b8}',0,'','2026-10-14 17:06:11','2026-10-14 17:06:11',2),(186,2,5,2,'hi',0,'{ce56acfd-959c-4924-92d9-8dbf0df8c934}',0,'','2026-10-14 17:53:20','2026-10-14 17:53:20',2),(187,2,5,2,'hi',0,'{6263afda-0df3-4284-8c32-238f9fb91cf4}',0,'','2026-10-14 17:53:29','2026-10-14 17:53:29',2),(188,2,5,2,'????',0,'{1966c116-92e4-4203-86a2-d22d44dc51fc}',0,'','2026-10-14 19:39:06','2026-10-14 19:39:06',2),(189,2,5,2,'?',0,'{b23500fe-b320-4674-8438-f25cca5305f4}',0,'','2026-10-14 20:41:31','2026-10-14 20:41:31',2),(190,2,5,2,'{\"name\":\"老人与海.m4a\",\"size\":4806482,\"url\":\"upload/1791986710000_4e5bbc9d818aa852.m4a\"}',2,'{62006042-f62e-405c-9025-b92914eb0765}',0,'','2026-10-14 22:05:10','2026-10-14 22:05:10',2),(191,6,5,3,'{\"name\":\"老人与海(1).m4a\",\"size\":4806482,\"url\":\"upload/1791986723646_4e5bbc9d818aa852.m4a\"}',2,'{008acc14-cc4e-4715-bfa2-b1bfa45fb4df}',0,'','2026-10-14 22:05:23','2026-10-14 22:05:23',2),(192,6,3,5,'{\"name\":\"无限试用脚本.zip\",\"size\":460,\"url\":\"upload/1791988015325_3895cb5c807157a6.zip\"}',2,'{5357736b-3d84-4beb-81a6-1498f71c691c}',0,'','2026-10-14 22:26:55','2026-10-14 22:26:55',2),(193,1,3,2,'额',0,'{6259708e-a230-459f-a6b1-b5a192688b28}',0,'','2026-10-14 23:10:47','2026-10-14 23:10:47',2),(194,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:43:42','2026-10-16 20:43:42',2),(195,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:43:42','2026-10-16 20:43:42',2),(200,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:46:08','2026-10-16 20:46:08',2),(201,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:46:09','2026-10-16 20:46:09',2),(204,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:46:34','2026-10-16 20:46:34',2),(205,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:46:34','2026-10-16 20:46:34',2),(208,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:46:47','2026-10-16 20:46:47',2),(209,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:46:48','2026-10-16 20:46:48',2),(212,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:47:01','2026-10-16 20:47:01',2),(213,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:47:01','2026-10-16 20:47:01',2),(216,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:50:57','2026-10-16 20:50:57',2),(217,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:50:57','2026-10-16 20:50:57',2),(220,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:51:10','2026-10-16 20:51:10',2),(221,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:51:11','2026-10-16 20:51:11',2),(224,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:52:10','2026-10-16 20:52:10',2),(225,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:52:10','2026-10-16 20:52:10',2),(228,1,2,3,'',0,'u_empty',0,'','2026-10-16 20:52:23','2026-10-16 20:52:23',2),(229,1,2,3,'hi',0,'u_z',0,'','2026-10-16 20:52:23','2026-10-16 20:52:23',2),(230,6,3,5,'{\"name\":\"2-图论作业.pdf\",\"size\":240388,\"url\":\"upload/1792163109211_34d2fc57166d5e67.pdf\"}',2,'{1cac32bb-0408-4dbb-9b32-62b6a4ce6cc2}',0,'','2026-10-16 23:05:09','2026-10-16 23:05:09',2),(231,1,3,2,'?',0,'{feb414ee-c151-4caf-a9b1-349e47ba3170}',0,'','2026-10-17 10:00:42','2026-10-17 10:00:42',2),(232,1,3,2,'?',0,'{83638c6b-c163-47f2-81fb-fd40375e87f5}',0,'','2026-10-17 10:01:09','2026-10-17 10:01:09',2),(233,6,3,5,'{\"name\":\"dsh-latest-windows-x64.exe\",\"size\":289313640,\"url\":\"upload/1792217598263_6a6c9e9e15faf836.bin\"}',2,'{bd43f36a-7e2f-4bc4-b251-2c0f8e67ae2a}',0,'','2026-10-17 14:13:18','2026-10-17 14:13:18',2),(234,1,2,3,'[AUTOTEST] thread归属-合法消息',0,'ut_ok',0,'','2026-10-17 15:37:38','2026-10-17 15:37:38',2),(235,1,2,3,'[AUTOTEST] thread归属-伪造touid',0,'ut_touid',0,'','2026-10-17 15:37:38','2026-10-17 15:37:38',2),(236,1,2,3,'',0,'u_empty',0,'','2026-10-17 15:37:41','2026-10-17 15:37:41',2),(237,1,2,3,'hi',0,'u_z',0,'','2026-10-17 15:37:41','2026-10-17 15:37:41',2),(238,1,2,3,'[AUTOTEST] thread归属-合法消息',0,'ut_ok',0,'','2026-10-17 15:38:58','2026-10-17 15:38:58',2),(239,1,2,3,'[AUTOTEST] thread归属-伪造touid',0,'ut_touid',0,'','2026-10-17 15:38:59','2026-10-17 15:38:59',2),(240,1,2,3,'',0,'u_empty',0,'','2026-10-17 15:39:01','2026-10-17 15:39:01',2),(241,1,2,3,'hi',0,'u_z',0,'','2026-10-17 15:39:02','2026-10-17 15:39:02',2),(242,1,2,3,'[AUTOTEST] thread归属-合法消息',0,'ut_ok',0,'','2026-10-17 16:36:27','2026-10-17 16:36:27',2),(243,1,2,3,'[AUTOTEST] thread归属-伪造touid',0,'ut_touid',0,'','2026-10-17 16:36:27','2026-10-17 16:36:27',2),(244,1,2,3,'',0,'u_empty',0,'','2026-10-17 16:36:30','2026-10-17 16:36:30',2),(245,1,2,3,'hi',0,'u_z',0,'','2026-10-17 16:36:30','2026-10-17 16:36:30',2),(246,1,2,3,'[AUTOTEST] thread归属-合法消息',0,'ut_ok',0,'','2026-10-17 18:38:19','2026-10-17 18:38:19',2),(247,1,2,3,'[AUTOTEST] thread归属-伪造touid',0,'ut_touid',0,'','2026-10-17 18:38:20','2026-10-17 18:38:20',2),(248,1,2,3,'',0,'u_empty',0,'','2026-10-17 18:38:22','2026-10-17 18:38:22',2),(249,1,2,3,'hi',0,'u_z',0,'','2026-10-17 18:38:23','2026-10-17 18:38:23',2),(250,1,2,3,'[AUTOTEST] thread归属-合法消息',0,'ut_ok',0,'','2026-10-17 18:42:52','2026-10-17 18:42:52',2),(251,1,2,3,'[AUTOTEST] thread归属-伪造touid',0,'ut_touid',0,'','2026-10-17 18:42:52','2026-10-17 18:42:52',2),(252,1,2,3,'',0,'u_empty',0,'','2026-10-17 18:42:55','2026-10-17 18:42:55',2),(253,1,2,3,'hi',0,'u_z',0,'','2026-10-17 18:42:55','2026-10-17 18:42:55',2),(255,21,2,0,'群里的第一条消息',0,'ad736dad-12f2-4429-aa0f-0cc2da44d282',0,'','2026-10-17 23:16:19','2026-10-17 23:16:19',2),(256,21,2,0,'这条应该扇出给成员',0,'9e535bcb-8746-4424-ba3b-758a1aa71921',0,'','2026-10-17 23:16:20','2026-10-17 23:16:20',2),(257,21,2,0,'自己不该收到这条',0,'58a81833-86cf-4577-8da9-dc9a9466a2c8',0,'','2026-10-17 23:16:20','2026-10-17 23:16:20',2),(258,21,2,0,'{\"name\":\"config.ini\",\"size\":265,\"url\":\"upload/1792299787052_d17e28337d182b6f.ini\"}',2,'{3acd3bbc-b0cf-465e-bcc4-af70b070b48a}',0,'','2026-10-18 13:03:07','2026-10-18 13:03:07',2),(259,17,2,0,'HI',0,'{cffdd5ce-6529-4ff8-b6f3-4bf6d6a571a0}',0,'','2026-10-18 13:03:15','2026-10-18 13:03:15',2),(260,1,2,3,'[AUTOTEST] thread归属-合法消息',0,'ut_ok',0,'','2026-10-18 20:19:12','2026-10-18 20:19:12',2),(261,1,2,3,'[AUTOTEST] thread归属-伪造touid',0,'ut_touid',0,'','2026-10-18 20:19:12','2026-10-18 20:19:12',2),(262,1,2,3,'',0,'u_empty',0,'','2026-10-18 20:19:15','2026-10-18 20:19:15',2),(263,1,2,3,'hi',0,'u_z',0,'','2026-10-18 20:19:15','2026-10-18 20:19:15',2),(279,1,2,3,'[AUTOTEST] thread归属-合法消息',0,'ut_ok',0,'','2026-10-21 20:37:08','2026-10-21 20:37:08',2),(280,1,2,3,'[AUTOTEST] thread归属-伪造touid',0,'ut_touid',0,'','2026-10-21 20:37:08','2026-10-21 20:37:08',2),(281,1,2,3,'',0,'u_empty',0,'','2026-10-21 20:37:11','2026-10-21 20:37:11',2),(282,1,2,3,'hi',0,'u_z',0,'','2026-10-21 20:37:11','2026-10-21 20:37:11',2),(286,31,5,6,'在吗？想加你好友',0,NULL,0,NULL,'2026-10-24 19:24:59','2026-10-24 19:24:59',2),(287,31,6,5,'We are friends now!',0,NULL,0,NULL,'2026-10-24 19:24:59','2026-10-24 19:24:59',2),(288,32,6,0,'？\n',0,'{71eca165-a830-4c59-acd9-6b6b4b13293a}',0,'','2026-10-24 20:56:11','2026-10-24 20:56:11',2),(289,32,2,0,'.。。',0,'{84c8626c-53d4-45ae-a531-e37c555205a6}',0,'','2026-10-24 20:56:26','2026-10-24 20:56:26',2);
/*!40000 ALTER TABLE `chat_message` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `chat_thread`
--

DROP TABLE IF EXISTS `chat_thread`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `chat_thread` (
  `thread_id` int NOT NULL AUTO_INCREMENT,
  `type` varchar(32) NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`thread_id`)
) ENGINE=InnoDB AUTO_INCREMENT=33 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `chat_thread`
--

LOCK TABLES `chat_thread` WRITE;
/*!40000 ALTER TABLE `chat_thread` DISABLE KEYS */;
INSERT INTO `chat_thread` VALUES (1,'private','2026-10-10 15:16:43'),(2,'private','2026-10-10 15:16:43'),(3,'private','2026-10-10 15:16:43'),(4,'private','2026-10-10 15:16:43'),(5,'private','2026-10-10 15:16:43'),(6,'private','2026-10-10 15:16:43'),(7,'private','2026-10-10 15:16:43'),(8,'private','2026-10-10 15:16:43'),(9,'private','2026-10-10 15:16:43'),(10,'private','2026-10-10 15:16:43'),(11,'private','2026-10-10 15:16:43'),(12,'private','2026-10-10 15:16:43'),(13,'private','2026-10-10 15:16:43'),(14,'private','2026-10-10 15:16:43'),(15,'private','2026-10-10 15:16:43'),(17,'group','2026-10-17 15:56:27'),(21,'group','2026-10-17 23:16:18'),(23,'group','2026-10-18 13:19:40'),(24,'group','2026-10-18 13:19:45'),(31,'private','2026-10-24 19:24:59'),(32,'group','2026-10-24 20:55:42');
/*!40000 ALTER TABLE `chat_thread` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `friend`
--

DROP TABLE IF EXISTS `friend`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `friend` (
  `self_id` int NOT NULL,
  `friend_id` int NOT NULL,
  `back` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`self_id`,`friend_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `friend`
--

LOCK TABLES `friend` WRITE;
/*!40000 ALTER TABLE `friend` DISABLE KEYS */;
INSERT INTO `friend` VALUES (2,3,'xxxl'),(2,5,'test1999'),(2,6,'test1998'),(2,7,'test1997'),(2,8,'text1996'),(3,2,'testuser'),(3,5,'test1999'),(3,6,'test1998'),(3,7,'test1997'),(3,8,'text1996'),(5,2,'man'),(5,3,'mimimmi'),(5,6,'67小子'),(5,7,'虎哥'),(5,8,'皮特大神'),(6,2,'testuser'),(6,3,'xxxl'),(6,5,'test1999'),(6,7,'test1997'),(6,8,'text1996'),(7,2,'testuser'),(7,3,'xxxl'),(7,5,'test1999'),(7,6,'test1998'),(7,8,'text1996'),(8,2,'testuser'),(8,3,'xxxl'),(8,5,'test1999'),(8,6,'test1998'),(8,7,'test1997');
/*!40000 ALTER TABLE `friend` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `friend_apply`
--

DROP TABLE IF EXISTS `friend_apply`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `friend_apply` (
  `id` int NOT NULL AUTO_INCREMENT,
  `from_uid` int NOT NULL,
  `to_uid` int NOT NULL,
  `descs` varchar(255) DEFAULT NULL,
  `back_name` varchar(255) DEFAULT NULL,
  `status` int NOT NULL DEFAULT '0',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_from_to` (`from_uid`,`to_uid`),
  KEY `idx_to` (`to_uid`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `friend_apply`
--

LOCK TABLES `friend_apply` WRITE;
/*!40000 ALTER TABLE `friend_apply` DISABLE KEYS */;
INSERT INTO `friend_apply` VALUES (4,2,3,'我是 testuser，交个朋友吧~','xxxl',1,'2026-10-10 14:56:43'),(5,5,6,'在吗？想加你好友','test1998',1,'2026-10-10 15:11:43'),(6,7,8,'已经加过了','text1996',1,'2026-10-07 15:16:43');
/*!40000 ALTER TABLE `friend_apply` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `group_chat`
--

DROP TABLE IF EXISTS `group_chat`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `group_chat` (
  `thread_id` int NOT NULL,
  `name` varchar(64) NOT NULL DEFAULT '',
  `owner_id` int NOT NULL DEFAULT '0',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`thread_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `group_chat`
--

LOCK TABLES `group_chat` WRITE;
/*!40000 ALTER TABLE `group_chat` DISABLE KEYS */;
INSERT INTO `group_chat` VALUES (17,'Hcat 测试群',2,'2026-10-17 15:56:27'),(21,'群聊用例_1792250178',2,'2026-10-17 23:16:18'),(23,'AAA',2,'2026-10-18 13:19:40'),(24,'AAA',2,'2026-10-18 13:19:45'),(32,'六座',7,'2026-10-24 20:55:42');
/*!40000 ALTER TABLE `group_chat` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `group_chat_member`
--

DROP TABLE IF EXISTS `group_chat_member`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `group_chat_member` (
  `thread_id` int NOT NULL,
  `user_id` int NOT NULL,
  PRIMARY KEY (`thread_id`,`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `group_chat_member`
--

LOCK TABLES `group_chat_member` WRITE;
/*!40000 ALTER TABLE `group_chat_member` DISABLE KEYS */;
INSERT INTO `group_chat_member` VALUES (17,2),(17,3),(17,5),(17,6),(17,7),(17,8),(21,2),(21,3),(21,5),(21,6),(21,8),(23,2),(24,2),(32,2),(32,3),(32,5),(32,6),(32,7),(32,8);
/*!40000 ALTER TABLE `group_chat_member` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `private_chat`
--

DROP TABLE IF EXISTS `private_chat`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `private_chat` (
  `thread_id` int NOT NULL,
  `user1_id` int NOT NULL,
  `user2_id` int NOT NULL,
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`thread_id`),
  KEY `idx_u1` (`user1_id`),
  KEY `idx_u2` (`user2_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `private_chat`
--

LOCK TABLES `private_chat` WRITE;
/*!40000 ALTER TABLE `private_chat` DISABLE KEYS */;
INSERT INTO `private_chat` VALUES (1,2,3,'2026-10-10 15:16:43'),(2,2,5,'2026-10-10 15:16:43'),(3,2,6,'2026-10-10 15:16:43'),(4,2,7,'2026-10-10 15:16:43'),(5,2,8,'2026-10-10 15:16:43'),(6,3,5,'2026-10-10 15:16:43'),(7,3,6,'2026-10-10 15:16:43'),(8,3,7,'2026-10-10 15:16:43'),(9,3,8,'2026-10-10 15:16:43'),(10,5,6,'2026-10-10 15:16:43'),(11,5,7,'2026-10-10 15:16:43'),(12,5,8,'2026-10-10 15:16:43'),(13,6,7,'2026-10-10 15:16:43'),(14,6,8,'2026-10-10 15:16:43'),(15,7,8,'2026-10-10 15:16:43'),(31,6,5,'2026-10-24 19:24:59');
/*!40000 ALTER TABLE `private_chat` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `user`
--

DROP TABLE IF EXISTS `user`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `user` (
  `id` int NOT NULL AUTO_INCREMENT,
  `uid` int NOT NULL,
  `name` varchar(255) NOT NULL,
  `email` varchar(255) NOT NULL,
  `pwd` varchar(255) NOT NULL,
  `nick` varchar(255) DEFAULT NULL,
  `desc` varchar(255) DEFAULT NULL,
  `sex` int NOT NULL DEFAULT '0',
  `icon` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=17 DEFAULT CHARSET=utf8mb3;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `user`
--

LOCK TABLES `user` WRITE;
/*!40000 ALTER TABLE `user` DISABLE KEYS */;
INSERT INTO `user` VALUES (1,2,'mimimi','0@1.com','745230','河南神','你才不幸福',1,'upload/1792585256903_da79caba34d0a360.png'),(2,3,'xxxl','seed@hchat.demo','745230','甜菜','HI!',1,'upload/1791688785510_20fe3108a2a817d0.png'),(10,5,'10599','1@qq.com','745230','上善若水','我是说唱皮特',2,'upload/1791703483561_59a7d487ac0dcfd6.png'),(11,6,'11699','1@1.com','745230','2246','6767676767',0,':/res/head_2.jpg'),(12,7,'16799','2@1.com','745230','叫我胡歌','我徒弟呢？！',0,':/res/head_3.jpg'),(13,8,'13899','3@1.com','745230','Peter','I‘m Peter',0,':/res/head_4.jpg');
/*!40000 ALTER TABLE `user` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `user_id`
--

DROP TABLE IF EXISTS `user_id`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `user_id` (
  `id` int NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb3;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `user_id`
--

LOCK TABLES `user_id` WRITE;
/*!40000 ALTER TABLE `user_id` DISABLE KEYS */;
INSERT INTO `user_id` VALUES (8);
/*!40000 ALTER TABLE `user_id` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-10-25 18:37:12
