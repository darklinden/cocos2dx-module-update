//-------------------- Module Update ----------------------
	public static void extractSrcZip(String srcPath, String desPath) {
		try {
			File destinationFile = new File(desPath);

			if (srcPath.contains("assets/")) {
				srcPath = srcPath.substring("assets/".length());
			}

			FileOutputStream outputStream = new FileOutputStream(
					destinationFile);
			InputStream inputStream = appActivity.getAssets().open(srcPath);
			byte[] buffer = new byte[1024];
			int length = 0;
			while ((length = inputStream.read(buffer)) != -1) {
				outputStream.write(buffer, 0, length);
			}
			outputStream.close();
			inputStream.close();

			String command = "chmod 777 " + desPath;
			Runtime runtime = Runtime.getRuntime();
			runtime.exec(command);
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
